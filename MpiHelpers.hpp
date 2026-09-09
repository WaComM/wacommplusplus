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
        constexpr std::size_t num_members = 9;
        int lengths[num_members] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        MPI_Aint offsets[num_members] = {
                offsetof(struct particle_data, id),
                offsetof(struct particle_data, k),
                offsetof(struct particle_data, j),
                offsetof(struct particle_data, i),
                offsetof(struct particle_data, health),
                offsetof(struct particle_data, age),
                offsetof(struct particle_data, time),
                offsetof(struct particle_data, driftObjectType),
                offsetof(struct particle_data, driftSide)
        };
        MPI_Datatype types[num_members] = {
                MPI_UINT64_T,
                MPI_DOUBLE,
                MPI_DOUBLE,
                MPI_DOUBLE,
                MPI_DOUBLE,
                MPI_DOUBLE,
                MPI_DOUBLE,
                MPI_UINT16_T,
                MPI_INT8_T
        };
        MPI_Datatype fields,particleType;
        MPI_Type_create_struct(num_members, lengths, offsets, types, &fields);
        MPI_Type_create_resized(fields,0,sizeof(struct particle_data),&particleType);
        MPI_Type_commit(&particleType);
        MPI_Type_free(&fields);
        return particleType;
    }
}

#endif //WACOMMPLUSPLUS_MPIHELPERS_HPP
