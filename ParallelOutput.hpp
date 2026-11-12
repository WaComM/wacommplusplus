//
// Created by Raffaele Montella on 12/11/26.
//

#ifndef WACOMMPLUSPLUS_PARALLELOUTPUT_HPP
#define WACOMMPLUSPLUS_PARALLELOUTPUT_HPP

#include "ParallelIOSupport.hpp"

#ifdef WACOMM_PARALLEL_NETCDF

#define OMPI_SKIP_MPICXX
#include <mpi.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Array.h"
#include "Particles.hpp"
#include "OceanModelAdapter.hpp"
#include "Config.hpp"

/*
 * Collective output (io.parallel_write="collective", docs/io.md#collective-writes).
 *
 * Every function is collective over comm: all ranks call it with the same file name.
 * The particle set or the concentration array exists on rank 0 only (it gathers the
 * particles for concentration evaluation); it is redistributed in contiguous blocks
 * and each rank writes its block, so storage traffic is spread over all ranks.
 *
 * Text histories are written entirely with MPI-IO (MPI_File_write_at_all). NetCDF
 * files are created by rank 0 with the serial schema writer, which also writes the
 * small coordinate variables; the particle-state and concentration variables, which
 * dominate the file size, are then written with parallel NetCDF-4 in NC_COLLECTIVE
 * mode. Files are published atomically as in OutputWriter. The values written equal
 * the serial writers exactly.
 */
namespace ParallelOutput {

    // Balanced contiguous blocks: the first n%size ranks receive one extra element
    void blocks(std::size_t n, int size, std::vector<int> &counts, std::vector<int> &displs);

    // Text history (Particles::saveAsTxt layout) with MPI-IO
    void writeTextHistory(MPI_Comm comm, const std::string &fileName, Particles *rootParticles,
                          bool fsync, const std::map<std::string,std::string> &hints);

    // NetCDF history (Particles::saveAsNetCDF schema) with parallel NetCDF-4
    void writeNetCDFHistory(MPI_Comm comm, const std::string &fileName, Particles *rootParticles,
                            double particleTime, std::shared_ptr<OceanModelAdapter> oceanModelAdapter,
                            std::shared_ptr<Config> config, bool fsync,
                            const std::map<std::string,std::string> &hints);

    // Gridded counts: rank 0 defines the file with rootDefine(path); conc is split along eta_rho
    void writeConcentration(MPI_Comm comm, const std::string &fileName, Array::Array4<float> *rootConc,
                            std::shared_ptr<OceanModelAdapter> oceanModelAdapter,
                            std::function<void(const std::string&)> rootDefine, bool fsync,
                            const std::map<std::string,std::string> &hints);

    // Broadcast a string from rank 0
    void broadcast(MPI_Comm comm, std::string &value);
}

#endif

#endif //WACOMMPLUSPLUS_PARALLELOUTPUT_HPP
