//
// Created by Raffaele Montella on 9/9/26.
//

#ifndef WACOMMPLUSPLUS_CONCENTRATION_HPP
#define WACOMMPLUSPLUS_CONCENTRATION_HPP

#include "Array.h"
#include "Particles.hpp"

#include <cmath>
#include <cstddef>

namespace Concentration {

    inline void evaluate(Particles *particles, int concentrationTimeIdx, size_t s_rho,
                         size_t eta_rho, size_t xi_rho, Array4<float> &conc) {
        size_t nParticles = particles->size();

        // Evaluate the concentration of particles per grid cell
        #pragma omp parallel for default(none) shared(particles, nParticles, concentrationTimeIdx, s_rho, eta_rho, xi_rho, conc)
        // For each particle...
        for (int idx = 0; idx < nParticles; idx++) {
            // Get the reference to the particle
            const Particle &particle = particles->at(idx);

            // Even if it is redundant, check if the particle is alive
            if (particle.isAlive()) {

                // Get the integer indeces k, j, i
                int k = (int) round(particle.K());
                int j = (int) round(particle.J());
                int i = (int) round(particle.I());

                // Check if the indices are consistent
                if (j >= 0 && j < eta_rho && i >= 0 && i < xi_rho && k >= (-(int) s_rho + 1) && k <= 0) {

                    // Increment the count of the particles in the grid cell
                    #pragma omp atomic update
                    conc(concentrationTimeIdx, k, j, i) = conc(concentrationTimeIdx, k, j, i) + 1;
                }
            }
        }
    }
}

#endif //WACOMMPLUSPLUS_CONCENTRATION_HPP
