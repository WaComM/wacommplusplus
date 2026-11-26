// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/config.hxx"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"
#include "log4cplus/consoleappender.h"
#include "LogInitializer.hpp"
#include "log4cplus/layout.h"
#include "log4cplus/version.h"

#include "WacommPlusPlus.hpp"
#include "Particles.hpp"
#include "Sources.hpp"
#include "OceanModelAdapters/ROMSAdapter.hpp"
#include "JulianDate.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <iostream>
#include <memory>
#include <stdlib.h> /* getenv */
#include <string>
#include <vector>

#ifdef USE_CUDA
#include <cuda.h>
#include <cuda_runtime_api.h>
#endif

#ifdef USE_OMP
#include <omp.h>
#endif

#include "Utils.hpp"

#if defined(USE_MPI) || defined(USE_EMPI)
#define OMPI_SKIP_MPICXX
#include <mpi.h>
#endif

#ifdef USE_EMPI
extern "C" {
#include <empi.h>
}
#endif

using namespace std;


log4cplus::Logger logger;

int main(int argc, char **argv) {
    // Shut log4cplus down before main returns (LogInitializer.hpp)
    LogInitializer logInitializer;
    int num_gpus = 0;
    int world_size=1, world_rank=0;
    int ompMaxThreads=1;

#ifdef USE_OMP
    // The the number of threads
    ompMaxThreads=omp_get_max_threads();
#endif

#ifdef USE_MPI
    // Initialize MPI: OpenMP workers and the asynchronous output writer never call MPI
    int mpiThreadSupport=MPI_THREAD_SINGLE;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &mpiThreadSupport);

    // Get the number of involved processes
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Get the number of the current process (world_rank=0 is for the main process)
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
#endif

#ifdef USE_EMPI
    // Initialize EMPI
    MPI_Init(&argc, &argv);

    // Get the number of involved processes
    MPI_Comm_size(ADM_COMM_WORLD, &world_size);

    // Get the number of the current process (world_rank=0 is for the main process)
    MPI_Comm_rank(ADM_COMM_WORLD, &world_rank);
#endif

#ifdef USE_CUDA
    if (cudaGetDeviceCount(&num_gpus) != cudaSuccess) num_gpus = 0;
#endif

#if defined(USE_CUDA) && defined(USE_MPI)
    /*
     * Every rank uploads its own forcing copy to each GPU it sees, so ranks of one node that see the
     * same device repeat that upload over one PCIe link and multiply its device memory. Identify each
     * visible device by its PCI address and count the ranks of this node that share the busiest one.
     */
    int sharedGpuRanks = 0;
    long long sharedGpu = -1;
    {
        MPI_Comm nodeComm;
        MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, world_rank, MPI_INFO_NULL, &nodeComm);
        int nodeRank = 0, nodeSize = 1;
        MPI_Comm_rank(nodeComm, &nodeRank);
        MPI_Comm_size(nodeComm, &nodeSize);

        std::vector<long long> devices;
        for (int i = 0; i < num_gpus; i++) {
            cudaDeviceProp prop;
            if (cudaGetDeviceProperties(&prop, i) == cudaSuccess) {
                devices.push_back(((long long) prop.pciDomainID << 32) | (prop.pciBusID << 8) | prop.pciDeviceID);
            }
        }
        int count = (int) devices.size();
        std::vector<int> counts(nodeSize), displs(nodeSize);
        MPI_Gather(&count, 1, MPI_INT, counts.data(), 1, MPI_INT, 0, nodeComm);
        int total = 0;
        for (int i = 0; i < nodeSize; i++) { displs[i] = total; total += counts[i]; }
        std::vector<long long> all(std::max(total, 1));
        MPI_Gatherv(devices.data(), count, MPI_LONG_LONG, all.data(), counts.data(), displs.data(), MPI_LONG_LONG, 0, nodeComm);
        if (nodeRank == 0) {
            for (int i = 0; i < total; i++) {
                int ranks = (int) std::count(all.begin(), all.begin() + total, all[i]);
                if (ranks > sharedGpuRanks) { sharedGpuRanks = ranks; sharedGpu = all[i]; }
            }
        }
        MPI_Comm_free(&nodeComm);
    }
#endif

    // This variables can be set via the command line.
    std::string configFile = "namelist.wacomm";
    configFile = "wacomm.json";

    // Flush every console write: a fatal error or a killed job must not lose buffered log lines
    std::cout << std::unitbuf;

    // Inizitalizer
    log4cplus::initialize();

    // Basic configuration
    log4cplus::BasicConfigurator basicConfigurator;
    basicConfigurator.configure();

    //Create an appender pointing to the console
    log4cplus::SharedAppenderPtr appender(new log4cplus::ConsoleAppender());

    //Set the layout of the appender
    appender->setName(LOG4CPLUS_TEXT("console"));

    log4cplus::tstring pattern = LOG4CPLUS_TEXT("%D{%y-%m-%d %H:%M:%S,%Q} %-5p %c");
#if LOG4CPLUS_VERSION >= LOG4CPLUS_MAKE_VERSION(2, 0, 0)
    appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
#else
    appender->setLayout(std::auto_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
#endif

    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));

    logger.addAppender(appender);

    // Set the logging level
#ifdef DEBUG
    log4cplus::LogLevel logLevel = log4cplus::DEBUG_LOG_LEVEL;
#else
    log4cplus::LogLevel logLevel = log4cplus::INFO_LOG_LEVEL;
#endif
    std::string logLevelString = Utils::getEnvVar("WACOMM_LOGLEVEL");
    if (!logLevelString.empty()) {
        logLevel = std::stoi(logLevelString);
    }
    logger.setLogLevel(logLevel);

    if (argc==2) {
        configFile=string(argv[1]);
    }

    // Check if the process is the main one
    if (world_rank == 0) {
        LOG4CPLUS_INFO(logger, "WaComM - C++ Version");

#ifdef USE_MPI
        LOG4CPLUS_INFO(logger, "Parallel: Distributed Memory");
#endif
#ifdef USE_EMPI
        LOG4CPLUS_INFO(logger, "Parallel: Distributed Memory with Malleability");
#endif
#ifdef USE_OMP
        LOG4CPLUS_INFO(logger, "Parallel: Shared Memory");
#endif
#ifndef USE_OMP
#ifndef USE_MPI
#ifndef USE_EMPI
        LOG4CPLUS_INFO(logger, "Parallel: None");
#endif
#endif
#endif
#ifdef USE_OPENACC
        LOG4CPLUS_INFO(logger, "Acceleration: OpenAcc");
#elif USE_CUDA
        LOG4CPLUS_INFO(logger, "Acceleration: CUDA " << num_gpus << " device(s)");
        for (int i=0; i<num_gpus; i++){
		cudaDeviceProp prop;
    		cudaGetDeviceProperties(&prop, i);
    		LOG4CPLUS_INFO(logger, "Device Number: " << i);
    		LOG4CPLUS_INFO(logger, "Device name: " <<  prop.name);
    		LOG4CPLUS_INFO(logger, "Memory Clock Rate (KHz): " << prop.memoryClockRate);
    		LOG4CPLUS_INFO(logger, "Memory Bus Width (bits): " << prop.memoryBusWidth);
    		LOG4CPLUS_INFO(logger, "Peak Memory Bandwidth (GB/s): " << 2.0*prop.memoryClockRate*(prop.memoryBusWidth/8)/1.0e6);
	}
#else
        LOG4CPLUS_INFO(logger, "Acceleration: None");
#endif

        LOG4CPLUS_INFO(logger, world_rank << ": Using 1/" << world_size << " processes, each on " << ompMaxThreads
                                          << " threads.");
        LOG4CPLUS_INFO(logger, "Configuration: " << configFile);
    }

#if defined(USE_CUDA) && defined(USE_MPI)
    // Reported by the first rank of each node
    if (sharedGpuRanks > 1) {
        char busId[32];
        std::snprintf(busId, sizeof(busId), "%04llx:%02llx:%02llx", sharedGpu >> 32, (sharedGpu >> 8) & 0xff, sharedGpu & 0xff);
        LOG4CPLUS_WARN(logger, world_rank << ": " << sharedGpuRanks << " MPI ranks on this node share the GPU at PCI "
                               << busId << "; each rank uploads its own forcing copy to it. Use one rank per GPU, "
                               << "for example by setting CUDA_VISIBLE_DEVICES for each rank.");
    }
#endif

    // Record start time
    auto start_whole = std::chrono::high_resolution_clock::now();

    try {
        // Load the configuration file
        auto config = std::make_shared<Config>(configFile);

        // Check if it is the main process
        if (world_rank == 0) {

            // Show a message if it is a dry run
            if (config->Dry()) {
                LOG4CPLUS_INFO(logger, "*** Dry run ***");
            }
#ifdef USE_MPI
            if (config->AsyncIO() && mpiThreadSupport<MPI_THREAD_FUNNELED) {
                LOG4CPLUS_WARN(logger, "The MPI library provides no MPI_THREAD_FUNNELED support; "
                                       "the asynchronous output writer runs without that guarantee");
            }
#endif
        }

        WacommPlusPlus wacommPlusPlus(config);

        wacommPlusPlus.run();
    } catch (const std::exception &e) {
        // Log the cause before stopping all processes
        LOG4CPLUS_ERROR(logger, world_rank << ": " << e.what());
        std::cout.flush();
        std::cerr.flush();
#if defined(USE_MPI) || defined(USE_EMPI)
        MPI_Abort(MPI_COMM_WORLD, 1);
#endif
        return 1;
    }

    auto end_whole = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsedLocal = end_whole - start_whole;
    // LOG4CPLUS_INFO(logger,  "Global Execution Time (sec): " << elapsedLocal.count() / 24);

#ifdef USE_MPI
    // Finalize MPI
    MPI_Finalize();
#endif
    return 0;

}
