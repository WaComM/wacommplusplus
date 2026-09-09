//
// Created by Raffaele Montella on 9/9/26.
//

#ifndef WACOMMPLUSPLUS_MPIHELPERS_HPP
#define WACOMMPLUSPLUS_MPIHELPERS_HPP

#define OMPI_SKIP_MPICXX
#include <mpi.h>

#include "Particle.hpp"

#include <cstddef>

namespace MpiHelpers {

    inline MPI_Datatype particleDataType() {
        constexpr std::size_t num_members = 7;
        int lengths[num_members] = { 1, 1, 1, 1, 1, 1, 1 };
        MPI_Aint offsets[num_members] = {
                offsetof(struct particle_data, id),
                offsetof(struct particle_data, k),
                offsetof(struct particle_data, j),
                offsetof(struct particle_data, i),
                offsetof(struct particle_data, health),
                offsetof(struct particle_data, age),
                offsetof(struct particle_data, time)
        };
        MPI_Datatype types[num_members] = {
                MPI_UINT64_T,
                MPI_DOUBLE,
                MPI_DOUBLE,
                MPI_DOUBLE,
                MPI_DOUBLE,
                MPI_DOUBLE,
                MPI_DOUBLE
        };
        MPI_Datatype particleType;
        MPI_Type_create_struct(num_members, lengths, offsets, types, &particleType);
        MPI_Type_commit(&particleType);
        return particleType;
    }
}

#endif //WACOMMPLUSPLUS_MPIHELPERS_HPP
