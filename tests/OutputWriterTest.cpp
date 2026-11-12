// Output publication contract (docs/io.md):
//   - synchronous and asynchronous modes publish byte-identical files;
//   - a file appears under its final name only after a complete write (no ".partial" left);
//   - the asynchronous queue is bounded, drains on flush, and reports the first error;
//   - a failed write publishes nothing and removes its temporary file;
//   - staging writes through a node-local directory and leaves it empty;
//   - NetCDF jobs hold the process-wide NetCDF mutex;
//   - Lustre layouts are applied with "lfs setstripe" and failures are fatal;
//   - the io.* configuration keys are validated and round-trip.

#include "../OutputWriter.hpp"
#include "../Config.hpp"
#include "../ParallelIOSupport.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

std::string read(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

void writeText(const std::string &path, const std::string &content) {
    std::ofstream output(path, std::ios::binary);
    output << content;
    output.close();
    if (!output) throw std::runtime_error("cannot write " + path);
}

bool noPartial(const fs::path &dir) {
    for (const auto &entry: fs::directory_iterator(dir))
        if (entry.path().extension()==".partial") return false;
    return true;
}

template<typename F> bool throws(F f, const std::string &fragment="") {
    try { f(); }
    catch (const std::exception &e) {
        if (fragment.empty() || std::string(e.what()).find(fragment)!=std::string::npos) return true;
        std::cerr << "unexpected message: " << e.what() << "\n";
        return false;
    }
    return false;
}

std::string content(int n) {
    std::ostringstream text;
    for (int i=0;i<1000;i++) text << n << " " << i << "\n";
    return text.str();
}

void publication(const fs::path &root) {
    fs::path syncDir=root/"sync", asyncDir=root/"async";
    fs::create_directories(syncDir); fs::create_directories(asyncDir);
    {
        OutputWriter::Options options;
        OutputWriter writer(options);
        for (int n=0;n<5;n++) {
            std::string text=content(n);
            writer.submit((syncDir/("history-"+std::to_string(n)+".txt")).string(),
                          [text](const std::string &path) { writeText(path, text); }, false);
        }
        writer.flush();
    }
    {
        OutputWriter::Options options;
        options.async=true; options.maxPending=2; options.fsync=true;
        OutputWriter writer(options);
        for (int n=0;n<5;n++) {
            std::string text=content(n);
            writer.submit((asyncDir/("history-"+std::to_string(n)+".txt")).string(),
                          [text](const std::string &path) {
                              std::this_thread::sleep_for(std::chrono::milliseconds(10));
                              writeText(path, text);
                          }, false);
        }
        writer.flush();
    }
    for (int n=0;n<5;n++) {
        std::string name="history-"+std::to_string(n)+".txt";
        assert(read(syncDir/name)==content(n));
        assert(read(asyncDir/name)==read(syncDir/name));
    }
    assert(noPartial(syncDir) && noPartial(asyncDir));
}

void backPressure(const fs::path &root) {
    fs::path dir=root/"pressure";
    fs::create_directories(dir);
    OutputWriter::Options options;
    options.async=true; options.maxPending=2;
    OutputWriter writer(options);

    std::promise<void> release;
    std::shared_future<void> gate=release.get_future().share();
    auto blocked=[gate](const std::string &path) { gate.wait(); writeText(path, "x"); };

    // One running and one queued job fill the queue
    writer.submit((dir/"a.txt").string(), blocked, false);
    writer.submit((dir/"b.txt").string(), blocked, false);

    // A third submission waits until a slot is free
    std::atomic<bool> submitted(false);
    std::thread third([&] {
        writer.submit((dir/"c.txt").string(), [](const std::string &path) { writeText(path, "c"); }, false);
        submitted=true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(!submitted);
    assert(!fs::exists(dir/"a.txt"));
    release.set_value();
    third.join();
    writer.flush();
    assert(submitted && fs::exists(dir/"a.txt") && fs::exists(dir/"b.txt") && read(dir/"c.txt")=="c");
}

void failures(const fs::path &root) {
    fs::path dir=root/"failures";
    fs::create_directories(dir);
    for (bool async: {false, true}) {
        OutputWriter::Options options;
        options.async=async;
        OutputWriter writer(options);
        std::string name=(dir/(std::string(async ? "async" : "sync")+"-failed.txt")).string();
        auto failing=[](const std::string &path) {
            writeText(path, "truncated");
            throw std::runtime_error("disk full");
        };
        if (async) {
            writer.submit(name, failing, false);
            assert(throws([&] { writer.flush(); }, "Unable to write " + name + ": disk full"));
            // The error is reported once; the writer keeps publishing later files
            writer.flush();
        } else {
            assert(throws([&] { writer.submit(name, failing, false); }, "disk full"));
        }
        assert(!fs::exists(name));

        // A writer that creates no file is an error, not an empty output
        std::string empty=(dir/(std::string(async ? "async" : "sync")+"-empty.txt")).string();
        assert(throws([&] { writer.submit(empty, [](const std::string &) {}, false); writer.flush(); },
                      "the writer produced no file"));
        assert(!fs::exists(empty));
    }

    // An asynchronous error is also reported at the next submission
    OutputWriter::Options options;
    options.async=true;
    OutputWriter writer(options);
    std::promise<void> release, done;
    std::shared_future<void> gate=release.get_future().share();
    writer.submit((dir/"first.txt").string(), [gate](const std::string &) {
        gate.wait();
        throw std::runtime_error("first");
    }, false);
    // The marker runs after the failure has been recorded
    writer.submit((dir/"marker.txt").string(), [&done](const std::string &path) {
        writeText(path, "m");
        done.set_value();
    }, false);
    release.set_value();
    done.get_future().wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(throws([&] {
        writer.submit((dir/"second.txt").string(), [](const std::string &path) { writeText(path, "2"); }, false);
    }, "first"));
    writer.flush();
    assert(noPartial(dir));
}

void staging(const fs::path &root) {
    fs::path dir=root/"pfs", local=root/"local-scratch";
    fs::create_directories(dir);
    OutputWriter::Options options;
    options.async=true; options.filesystem=OutputWriter::FILESYSTEM_GPFS; options.fsync=true;
    options.stagingDir=local.string();
    OutputWriter writer(options);
    std::atomic<bool> staged(true);
    for (int n=0;n<3;n++) {
        std::string text=content(n);
        writer.submit((dir/("conc-"+std::to_string(n)+".nc")).string(),
                      [text, local, &staged](const std::string &path) {
                          if (fs::path(path).parent_path()!=local) staged=false;
                          writeText(path, text);
                      }, false);
    }
    writer.flush();
    assert(staged);
    for (int n=0;n<3;n++) assert(read(dir/("conc-"+std::to_string(n)+".nc"))==content(n));
    assert(fs::is_empty(local) && noPartial(dir));
}

void netcdfSerialization(const fs::path &root) {
    OutputWriter::Options options;
    options.async=true;
    OutputWriter writer(options);
    std::atomic<bool> locked(false), unlocked(false);
    writer.submit((root/"locked.nc").string(), [&locked](const std::string &path) {
        // try_lock from the owning thread is undefined; probe from another thread
        std::thread probe([&locked] {
            locked=!OutputWriter::netcdfMutex().try_lock();
            if (!locked) OutputWriter::netcdfMutex().unlock();
        });
        probe.join();
        writeText(path, "nc");
    }, true);
    writer.submit((root/"unlocked.txt").string(), [&unlocked](const std::string &path) {
        unlocked=OutputWriter::netcdfMutex().try_lock();
        if (unlocked) OutputWriter::netcdfMutex().unlock();
        writeText(path, "txt");
    }, false);
    writer.flush();
    assert(locked && unlocked);
}

void lustre(const fs::path &root) {
    OutputWriter::Options options;
    options.lustreStripeCount=4;
    options.outputDirs={(root/"lustre").string()};
    // Striping on a file system that is not Lustre is a configuration error
    assert(throws([&] { OutputWriter writer(options); }, "require io.filesystem=lustre"));
#ifndef _WIN32
    // A fake lfs on PATH records its arguments
    fs::path bin=root/"bin", log=root/"lfs.log";
    fs::create_directories(bin);
    {
        std::ofstream script(bin/"lfs");
        script << "#!/bin/sh\necho \"$@\" >> '" << log.string() << "'\n"
               << "if [ -n \"$WACOMM_FAKE_LFS_FAIL\" ]; then exit 1; fi\n";
    }
    fs::permissions(bin/"lfs", fs::perms::owner_all);
    std::string path=bin.string()+":"+(std::getenv("PATH") ? std::getenv("PATH") : "");
    setenv("PATH", path.c_str(), 1);

    options.filesystem=OutputWriter::FILESYSTEM_LUSTRE;
    options.lustreStripeSize=1048576;
    { OutputWriter writer(options); }
    std::string expected="setstripe -c 4 -S 1048576 "+(root/"lustre").string()+"\n";
    assert(read(log)==expected);
    assert(fs::is_directory(root/"lustre"));

    // Inheriting the layout runs no command
    OutputWriter::Options inherit;
    inherit.filesystem=OutputWriter::FILESYSTEM_LUSTRE;
    inherit.outputDirs=options.outputDirs;
    { OutputWriter writer(inherit); }
    assert(read(log)==expected);

    setenv("WACOMM_FAKE_LFS_FAIL", "1", 1);
    assert(throws([&] { OutputWriter writer(options); }, "lfs setstripe failed"));
    unsetenv("WACOMM_FAKE_LFS_FAIL");
#endif
}

void configuration(const fs::path &root) {
    auto load=[&root](const std::string &io) {
        fs::path file=root/"config.json";
        std::ofstream(file) << R"({"simulation":{"start":"20210701Z00"},"io":)" << io << "}\n";
        return Config(file.string());
    };
    Config defaults=load("{}");
    assert(!defaults.AsyncIO() && defaults.AsyncMaxPending()==2 && defaults.Filesystem()=="posix");
    assert(!defaults.Fsync() && defaults.StagingDir().empty());
    assert(defaults.LustreStripeCount()==0 && defaults.LustreStripeSize()==0);

    // Parallel file systems flush before publication unless disabled
    assert(load(R"({"filesystem":"gpfs"})").Fsync());
    assert(!load(R"({"filesystem":"gpfs","fsync":false})").Fsync());

    Config parallel=load(R"({"async":true,"async_max_pending":3,"filesystem":"lustre",
                             "staging_dir":"/tmp/stage","lustre_stripe_count":-1,"lustre_stripe_size":4194304})");
    assert(parallel.AsyncIO() && parallel.AsyncMaxPending()==3 && parallel.Filesystem()=="lustre");
    assert(parallel.Fsync() && parallel.StagingDir()=="/tmp/stage");
    assert(parallel.LustreStripeCount()==-1 && parallel.LustreStripeSize()==4194304);

    // The resolved configuration round-trips
    parallel.saveAsJson((root/"saved.json").string());
    Config saved((root/"saved.json").string());
    assert(saved.AsyncIO() && saved.AsyncMaxPending()==3 && saved.Filesystem()=="lustre" && saved.Fsync());
    assert(saved.StagingDir()=="/tmp/stage" && saved.LustreStripeCount()==-1 && saved.LustreStripeSize()==4194304);

    for (const char *invalid: {R"({"async_max_pending":0})", R"({"async_max_pending":1.5})",
                               R"({"filesystem":"nfs"})", R"({"lustre_stripe_count":2})",
                               R"({"filesystem":"lustre","lustre_stripe_count":-2})",
                               R"({"filesystem":"lustre","lustre_stripe_size":1000})",
                               R"({"filesystem":"lustre","lustre_stripe_size":-65536})",
                               R"({"parallel_write":"pnetcdf"})",
                               R"({"mpiio_hints":{"romio_cb_write":"enable"}})",
                               R"({"parallel_write":"collective","mpiio_hints":{"cb_nodes":4}})",
                               R"({"parallel_write":"collective","mpiio_hints":["cb_nodes"]})"}) {
        assert(throws([&] { load(invalid); }));
    }

    // Collective output (docs/io.md#collective-writes)
    assert(defaults.ParallelWrite()=="none" && defaults.MpiioHints().empty());
    if (ParallelIOSupport::collectiveWrites) {
        Config collective=load(R"({"parallel_write":"collective","fsync":true,
                                   "mpiio_hints":{"romio_cb_write":"enable","striping_factor":"8"}})");
        assert(collective.ParallelWrite()=="collective" && collective.MpiioHints().size()==2);
        assert(collective.MpiioHints().at("striping_factor")=="8");
        collective.saveAsJson((root/"collective.json").string());
        Config reloaded((root/"collective.json").string());
        assert(reloaded.ParallelWrite()=="collective" && reloaded.MpiioHints()==collective.MpiioHints());
        assert(throws([&] { load(R"({"parallel_write":"collective","async":true})"); }, "io.async=false"));
        assert(throws([&] { load(R"({"parallel_write":"collective","staging_dir":"stage"})"); }, "staging_dir"));
        assert(throws([&] { load(R"({"parallel_write":"collective","save_history":"json"})"); }, "not json"));
    } else {
        // A build without MPI and parallel NetCDF rejects the option instead of writing serially
        assert(throws([&] { load(R"({"parallel_write":"collective"})"); }, "parallel HDF5"));
    }
}

}

int main() {
    fs::path root=fs::temp_directory_path()/("wacomm-output-writer-"+std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    publication(root);
    backPressure(root);
    failures(root);
    staging(root);
    netcdfSerialization(root);
    lustre(root);
    configuration(root);
    fs::remove_all(root);
    std::cout << "output writer contract passed\n";
    return 0;
}
