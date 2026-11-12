//
// Created by Raffaele Montella on 12/11/26.
//

#include "OutputWriter.hpp"
#include "Config.hpp"

#include <chrono>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <system_error>

#include "log4cplus/loggingmacros.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace fs = std::filesystem;

namespace {

    // Flush the data of a file to stable storage
    void syncFile(const fs::path &path) {
#ifdef _WIN32
        int fd=_open(path.string().c_str(), _O_RDWR | _O_BINARY);
        if (fd<0) throw std::runtime_error("cannot open " + path.string() + " to flush it");
        int status=_commit(fd);
        _close(fd);
        if (status!=0) throw std::runtime_error("cannot flush " + path.string());
#else
        int fd=::open(path.c_str(), O_RDONLY);
        if (fd<0) throw std::runtime_error("cannot open " + path.string() + " to flush it");
        int status;
#ifdef F_FULLFSYNC
        // macOS fsync does not flush the drive cache
        status=::fcntl(fd, F_FULLFSYNC);
        if (status!=0) status=::fsync(fd);
#else
        status=::fsync(fd);
#endif
        ::close(fd);
        if (status!=0) throw std::runtime_error("cannot flush " + path.string());
#endif
    }

    // Flush a directory entry (the rename) to stable storage; not available on Windows
    void syncDirectory(const fs::path &path) {
#ifndef _WIN32
        int fd=::open(path.c_str(), O_RDONLY);
        if (fd<0) throw std::runtime_error("cannot open directory " + path.string() + " to flush it");
        int status=::fsync(fd);
        ::close(fd);
        if (status!=0) throw std::runtime_error("cannot flush directory " + path.string());
#else
        (void)path;
#endif
    }

    fs::path parentDirectory(const fs::path &path) {
        fs::path parent=path.parent_path();
        return parent.empty() ? fs::path(".") : parent;
    }
}

OutputWriter::OutputWriter(Options options): opts(std::move(options)) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));

    if (opts.maxPending<1)
        throw std::runtime_error("io.async_max_pending must be at least 1");

    // Create the node-local staging directory
    if (!opts.stagingDir.empty()) {
        std::error_code ec;
        fs::create_directories(opts.stagingDir, ec);
        if (!fs::is_directory(opts.stagingDir))
            throw std::runtime_error("io.staging_dir is not a usable directory: " + opts.stagingDir);
    }

    // Set the default layout of the output directories before the first file is created
    applyLustreLayout();

    if (opts.async) {
        worker=std::thread(&OutputWriter::loop, this);
    }
    LOG4CPLUS_INFO(logger, "Output writer: mode=" << (opts.async ? "async" : "sync")
                           << " max_pending=" << opts.maxPending
                           << " filesystem=" << filesystemName(opts.filesystem)
                           << " fsync=" << (opts.fsync ? "true" : "false")
                           << " staging_dir=" << (opts.stagingDir.empty() ? "none" : opts.stagingDir));
}

static OutputWriter::Options optionsFromConfig(const Config &config) {
    OutputWriter::Options options;
    options.async=config.AsyncIO();
    options.maxPending=(std::size_t)config.AsyncMaxPending();
    options.filesystem=OutputWriter::filesystemFromName(config.Filesystem());
    options.fsync=config.Fsync();
    options.stagingDir=config.StagingDir();
    options.lustreStripeCount=config.LustreStripeCount();
    options.lustreStripeSize=config.LustreStripeSize();

    // Directories of the history and gridded output roots
    std::set<std::string> dirs;
    for (const std::string &root: {config.HistoryRoot(), config.NcOutputRoot()}) {
        dirs.insert(parentDirectory(fs::path(root)).string());
    }
    options.outputDirs.assign(dirs.begin(), dirs.end());
    return options;
}

OutputWriter::OutputWriter(const Config &config): OutputWriter(optionsFromConfig(config)) {}

OutputWriter::~OutputWriter() {
    if (worker.joinable()) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping=true;
        }
        changed.notify_all();

        // The worker drains the queue before stopping
        worker.join();
    }
    if (error) {
        try {
            std::rethrow_exception(error);
        } catch (const std::exception &e) {
            LOG4CPLUS_ERROR(logger, "Unreported output error: " << e.what());
        } catch (...) {
            LOG4CPLUS_ERROR(logger, "Unreported output error");
        }
    }
}

std::mutex& OutputWriter::netcdfMutex() {
    static std::mutex instance;
    return instance;
}

int OutputWriter::filesystemFromName(const std::string &name) {
    if (name=="posix") return FILESYSTEM_POSIX;
    if (name=="lustre") return FILESYSTEM_LUSTRE;
    if (name=="gpfs") return FILESYSTEM_GPFS;
    throw std::runtime_error("io.filesystem must be posix, lustre, or gpfs (received '" + name + "')");
}

std::string OutputWriter::filesystemName(int filesystem) {
    switch (filesystem) {
        case FILESYSTEM_LUSTRE: return "lustre";
        case FILESYSTEM_GPFS: return "gpfs";
        default: return "posix";
    }
}

void OutputWriter::applyLustreLayout() {
    if (opts.lustreStripeCount==0 && opts.lustreStripeSize==0) return;
    if (opts.filesystem!=FILESYSTEM_LUSTRE)
        throw std::runtime_error("io.lustre_stripe_count and io.lustre_stripe_size require io.filesystem=lustre");
#ifdef _WIN32
    throw std::runtime_error("Lustre striping is not available on Windows");
#else
    for (const std::string &dir: opts.outputDirs) {
        std::error_code ec;
        fs::create_directories(dir, ec);

        // lfs setstripe [-c count] [-S size] <directory>: new files inherit the layout
        std::vector<std::string> args={"lfs","setstripe"};
        if (opts.lustreStripeCount!=0) { args.push_back("-c"); args.push_back(std::to_string(opts.lustreStripeCount)); }
        if (opts.lustreStripeSize!=0) { args.push_back("-S"); args.push_back(std::to_string(opts.lustreStripeSize)); }
        args.push_back(dir);

        std::vector<char*> argv;
        for (auto &arg: args) argv.push_back(arg.data());
        argv.push_back(nullptr);

        pid_t pid;
        int status=posix_spawnp(&pid, "lfs", nullptr, nullptr, argv.data(), environ);
        if (status!=0)
            throw std::runtime_error("io.filesystem=lustre: cannot run lfs to set the layout of " + dir);
        int exitStatus=0;
        if (waitpid(pid, &exitStatus, 0)<0 || !WIFEXITED(exitStatus) || WEXITSTATUS(exitStatus)!=0)
            throw std::runtime_error("io.filesystem=lustre: lfs setstripe failed on " + dir);

        LOG4CPLUS_INFO(logger, "Lustre layout of " << dir << ": stripe_count=" << opts.lustreStripeCount
                               << " stripe_size=" << opts.lustreStripeSize);
    }
#endif
}

std::string OutputWriter::partialName(const std::string &fileName) {
    return fileName + ".partial";
}

void OutputWriter::commit(const std::string &partial, const std::string &fileName, bool fsync) {
    // Data first, then the name: a crash never publishes a file with unflushed data
    if (fsync) syncFile(partial);
    fs::rename(partial, fileName);
    if (fsync) syncDirectory(parentDirectory(fs::path(fileName)));
}

void OutputWriter::publish(const Job &job) {
    auto start = std::chrono::steady_clock::now();

    fs::path finalPath(job.fileName);
    fs::path destPartial(partialName(job.fileName));

    // Write in the staging directory when configured, otherwise next to the final file
    fs::path writePath=destPartial;
    if (!opts.stagingDir.empty()) {
        writePath=fs::path(opts.stagingDir) / finalPath.filename();
        writePath+=".partial";
    }

    std::error_code ec;
    fs::remove(writePath, ec);
    fs::remove(destPartial, ec);
    try {
        if (job.usesNetCDF) {
            std::lock_guard<std::mutex> lock(netcdfMutex());
            job.write(writePath.string());
        } else {
            job.write(writePath.string());
        }
        if (!fs::is_regular_file(writePath))
            throw std::runtime_error("the writer produced no file");

        // Copy the staged file into the output directory, then release the local copy
        if (writePath!=destPartial) {
            fs::copy_file(writePath, destPartial, fs::copy_options::overwrite_existing);
            fs::remove(writePath);
        }

        commit(destPartial.string(), finalPath.string(), opts.fsync);
    } catch (const std::exception &e) {
        fs::remove(writePath, ec);
        fs::remove(destPartial, ec);
        throw std::runtime_error("Unable to write " + job.fileName + ": " + e.what());
    }

    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    LOG4CPLUS_INFO(logger, "Published " << job.fileName << " in " << elapsed.count() << " seconds");
}

void OutputWriter::loop() {
    std::unique_lock<std::mutex> lock(mutex);
    while (true) {
        changed.wait(lock, [this] { return stopping || !queue.empty(); });
        if (queue.empty()) break;

        Job job=std::move(queue.front());
        queue.pop_front();
        running++;
        lock.unlock();

        std::exception_ptr failure;
        try {
            publish(job);
        } catch (...) {
            failure=std::current_exception();
        }

        lock.lock();
        running--;
        if (failure) {
            try {
                std::rethrow_exception(failure);
            } catch (const std::exception &e) {
                LOG4CPLUS_ERROR(logger, e.what());
            } catch (...) {}
            // The first error is reported to the solver thread
            if (!error) error=failure;
        }
        changed.notify_all();
    }
}

void OutputWriter::rethrowError() {
    std::exception_ptr failure;
    {
        std::lock_guard<std::mutex> lock(mutex);
        failure=error;
        error=nullptr;
    }
    if (failure) std::rethrow_exception(failure);
}

void OutputWriter::submit(const std::string &fileName, std::function<void(const std::string&)> write, bool usesNetCDF) {
    rethrowError();

    Job job{fileName, std::move(write), usesNetCDF};
    if (!opts.async) {
        publish(job);
        return;
    }

    std::unique_lock<std::mutex> lock(mutex);

    // Back-pressure: bound the memory held by the snapshots of queued writes
    auto start = std::chrono::steady_clock::now();
    bool waited=queue.size()+running>=opts.maxPending;
    changed.wait(lock, [this] { return queue.size()+running<opts.maxPending; });
    if (waited) {
        std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        LOG4CPLUS_INFO(logger, "Solver waited " << elapsed.count() << " seconds for the output queue");
    }

    queue.push_back(std::move(job));
    LOG4CPLUS_DEBUG(logger, "Queued " << fileName << " (" << queue.size()+running << " pending)");
    lock.unlock();
    changed.notify_all();
}

void OutputWriter::flush() {
    if (opts.async) {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [this] { return queue.empty() && running==0; });
    }
    rethrowError();
}
