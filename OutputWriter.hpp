//
// Created by Raffaele Montella on 12/11/26.
//

#ifndef WACOMMPLUSPLUS_OUTPUTWRITER_HPP
#define WACOMMPLUSPLUS_OUTPUTWRITER_HPP

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "log4cplus/logger.h"

class Config;

/*
 * Publishes particle histories, restarts, and gridded outputs (docs/io.md).
 *
 * Every file is written under a temporary ".partial" name and renamed to its final
 * name only after the writer returned without error, so a reader or a restart never
 * sees a truncated file. In asynchronous mode the writes run on one background thread
 * while the solver continues with the next forcing window; the caller hands over an
 * immutable snapshot of the state, so the file contents are identical to the
 * synchronous mode. The writer never changes physical time or particle state.
 */
class OutputWriter {
public:
    // File system layouts with a documented publication policy
    static const int FILESYSTEM_POSIX=0;
    static const int FILESYSTEM_LUSTRE=1;
    static const int FILESYSTEM_GPFS=2;

    struct Options {
        // Run the writes on a background thread
        bool async=false;

        // Maximum number of queued or running writes before submit() blocks
        std::size_t maxPending=2;

        // File system of the output directories
        int filesystem=FILESYSTEM_POSIX;

        // Flush file data (and the parent directory) to stable storage before publication
        bool fsync=false;

        // Optional node-local directory where files are written before being copied
        std::string stagingDir;

        // Lustre default layout of the output directories (0: inherit)
        int lustreStripeCount=0;
        long long lustreStripeSize=0;

        // Output directories receiving the Lustre layout
        std::vector<std::string> outputDirs;
    };

    explicit OutputWriter(Options options);
    explicit OutputWriter(const Config &config);
    ~OutputWriter();

    OutputWriter(const OutputWriter&)=delete;
    OutputWriter& operator=(const OutputWriter&)=delete;

    // Write fileName with write(path), where path is the temporary name to create.
    // usesNetCDF serializes the job with every other NetCDF call of the process.
    // An error of an earlier asynchronous job is rethrown here.
    void submit(const std::string &fileName, std::function<void(const std::string&)> write, bool usesNetCDF);

    // Wait for every queued write and rethrow the first error
    void flush();

    const Options& options() const { return opts; }

    // NetCDF-C and HDF5 are not thread safe: every NetCDF call of the process must hold it
    static std::mutex& netcdfMutex();

    // Parse a file system name ("posix", "lustre", "gpfs"); throws on unknown names
    static int filesystemFromName(const std::string &name);
    static std::string filesystemName(int filesystem);

private:
    struct Job {
        std::string fileName;
        std::function<void(const std::string&)> write;
        bool usesNetCDF;
    };

    log4cplus::Logger logger;
    Options opts;

    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Job> queue;
    std::size_t running=0;
    bool stopping=false;
    std::exception_ptr error;
    std::thread worker;

    void applyLustreLayout();
    void publish(const Job &job);
    void loop();
    void rethrowError();
};

#endif //WACOMMPLUSPLUS_OUTPUTWRITER_HPP
