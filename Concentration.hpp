//
// Created by Raffaele Montella on 9/9/26.
//

#ifndef WACOMMPLUSPLUS_CONCENTRATION_HPP
#define WACOMMPLUSPLUS_CONCENTRATION_HPP

#include "Array.h"
#include "Particles.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <cmath>
#include <cstddef>
#include <vector>

#ifdef USE_OMP
#include <omp.h>
#endif

namespace Concentration {

    // Rounded rho cell indices k, j, i of a living particle inside the grid
    inline bool cellIndices(const Particle &particle, size_t s_rho, size_t eta_rho, size_t xi_rho,
                            int &k, int &j, int &i) {

        // Even if it is redundant, check if the particle is alive
        if (!particle.isAlive()) return false;

        // Get the integer indeces k, j, i
        k = (int) round(particle.K());
        j = (int) round(particle.J());
        i = (int) round(particle.I());

        // Check if the indices are consistent
        return j >= 0 && j < eta_rho && i >= 0 && i < xi_rho && k >= (-(int) s_rho + 1) && k <= 0;
    }

    // Count the living particles per rho grid cell. Counts are integers, so the result does not depend
    // on the summation order: it is exact while every cell holds at most 2^24 particles.
    inline void evaluate(Particles *particles, int concentrationTimeIdx, size_t s_rho,
                         size_t eta_rho, size_t xi_rho, Array4<float> &conc) {
        size_t nParticles = particles->size();

#ifdef USE_OMP
        if (omp_get_max_threads() > 1 && nParticles > 1) {

            // Count cell of each particle in its thread's contiguous block, nullptr outside the grid;
            // left uninitialized so that the threads first touch their own pages
            std::unique_ptr<float *[]> cells(new float *[nParticles]);

            // The same cells grouped by the thread that owns them
            std::unique_ptr<float *[]> owned(new float *[nParticles]);

            // Thread t owns the cells from splitters[t-1] (included) to splitters[t] (excluded)
            std::vector<float *> splitters;

            // Particles of block u owned by thread r at counts[u*nThreads+r], then their position in owned
            std::vector<size_t> counts, ownedBegin;

            #pragma omp parallel default(none) shared(particles, nParticles, concentrationTimeIdx, s_rho, eta_rho, xi_rho, conc, cells, owned, splitters, counts, ownedBegin)
            {
                int nThreads = omp_get_num_threads();
                int t = omp_get_thread_num();

                // The block of a thread
                auto blockBegin = [&](int u) { return (size_t) ((unsigned long long) nParticles * u / nThreads); };

                // The thread that owns a cell
                auto owner = [&](float *cell) {
                    return (int) (std::upper_bound(splitters.begin(), splitters.end(), cell, std::less<float *>()) - splitters.begin());
                };

                // Find the count cell of each particle of this block
                for (size_t idx = blockBegin(t); idx < blockBegin(t + 1); idx++) {
                    int k, j, i;
                    cells[idx] = cellIndices(particles->at(idx), s_rho, eta_rho, xi_rho, k, j, i) ?
                                 &conc(concentrationTimeIdx, k, j, i) : nullptr;
                }

                #pragma omp barrier

                // Split the cells so that each thread owns about the same number of particles
                #pragma omp single
                {
                    std::vector<float *> samples;
                    size_t stride = std::max((size_t) 1, nParticles / (64 * (size_t) nThreads));
                    for (size_t idx = 0; idx < nParticles; idx += stride) {
                        if (cells[idx] != nullptr) samples.push_back(cells[idx]);
                    }
                    std::sort(samples.begin(), samples.end(), std::less<float *>());
                    splitters.clear();
                    for (int r = 1; r < nThreads && !samples.empty(); r++) {
                        splitters.push_back(samples[samples.size() * r / nThreads]);
                    }
                    counts.assign((size_t) nThreads * nThreads, 0);
                    ownedBegin.assign((size_t) nThreads * nThreads + 1, 0);
                }

                // Count the particles of this block per owner
                std::vector<size_t> local(nThreads, 0);
                for (size_t idx = blockBegin(t); idx < blockBegin(t + 1); idx++) {
                    if (cells[idx] != nullptr) local[owner(cells[idx])]++;
                }
                for (int r = 0; r < nThreads; r++) counts[(size_t) t * nThreads + r] = local[r];

                #pragma omp barrier

                // Group by owner, then by block
                #pragma omp single
                {
                    size_t position = 0;
                    for (int r = 0; r < nThreads; r++) {
                        for (int u = 0; u < nThreads; u++) {
                            ownedBegin[(size_t) u * nThreads + r] = position;
                            position += counts[(size_t) u * nThreads + r];
                        }
                    }
                    ownedBegin[(size_t) nThreads * nThreads] = position;
                }

                // Move the cells of this block to their owners
                for (int r = 0; r < nThreads; r++) local[r] = ownedBegin[(size_t) t * nThreads + r];
                for (size_t idx = blockBegin(t); idx < blockBegin(t + 1); idx++) {
                    if (cells[idx] != nullptr) owned[local[owner(cells[idx])]++] = cells[idx];
                }

                #pragma omp barrier

                // Increment the count of the particles in the grid cells of this thread: no other thread writes them
                size_t end = t + 1 < nThreads ? ownedBegin[(size_t) t + 1] : ownedBegin[(size_t) nThreads * nThreads];
                for (size_t idx = ownedBegin[(size_t) t]; idx < end; idx++) {
                    *owned[idx] = *owned[idx] + 1;
                }
            }
            return;
        }
#endif

        // Evaluate the concentration of particles per grid cell
        // For each particle...
        for (size_t idx = 0; idx < nParticles; idx++) {
            int k, j, i;
            if (cellIndices(particles->at(idx), s_rho, eta_rho, xi_rho, k, j, i)) {

                // Increment the count of the particles in the grid cell
                conc(concentrationTimeIdx, k, j, i) = conc(concentrationTimeIdx, k, j, i) + 1;
            }
        }
    }
}

#endif //WACOMMPLUSPLUS_CONCENTRATION_HPP
