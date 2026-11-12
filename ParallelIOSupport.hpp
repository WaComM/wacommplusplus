//
// Created by Raffaele Montella on 12/11/26.
//

#ifndef WACOMMPLUSPLUS_PARALLELIOSUPPORT_HPP
#define WACOMMPLUSPLUS_PARALLELIOSUPPORT_HPP

/*
 * Build capability of collective output (docs/io.md#collective-writes).
 *
 * Collective writes need the MPI decomposition (USE_MPI) and a NetCDF-C built with
 * parallel HDF5, which installs netcdf_par.h only in that case (for example the
 * private stack of WACOMM_BOOTSTRAP_PARALLEL_IO=ON). nc_open_par also rejects a
 * serial library at run time, so a mismatched header cannot silently write serially.
 */
#if defined(USE_MPI) && defined(__has_include)
#if __has_include(<netcdf_par.h>)
#define WACOMM_PARALLEL_NETCDF 1
#endif
#endif

namespace ParallelIOSupport {
#ifdef WACOMM_PARALLEL_NETCDF
    constexpr bool collectiveWrites=true;
#else
    constexpr bool collectiveWrites=false;
#endif
}

#endif //WACOMMPLUSPLUS_PARALLELIOSUPPORT_HPP
