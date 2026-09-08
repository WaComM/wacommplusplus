//
// Created by Raffaele Montella on 12/5/20.
//


#include <utility>
#include "Particle.hpp"
#include "Config.hpp"
#include "NumericalHelpers.hpp"

Particle::Particle(std::uint64_t id, double k, double j, double i,
                   double health, double age, double time)
{
#ifdef DEBUG
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));
#endif

    _data.id=id;
    _data.k=k;
    _data.j=j;
    _data.i=i;
    _data.health=health;
    _data.age=age;
    _data.time=time;

}

Particle::Particle(std::uint64_t id, double k, double j, double i, double time) {

#ifdef DEBUG
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));
#endif
    _data.id=id;
    _data.k=k;
    _data.j=j;
    _data.i=i;
    _data.health=health0;
    _data.age=0;
    _data.time=time;
}

Particle::Particle(particle_data data) {
#ifdef DEBUG
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));
#endif
    _data = data;
}

Particle::~Particle() = default;

particle_data Particle::data() {
    return _data;
}

void Particle::data(particle_data data) {
    _data = data;
}



bool Particle::isAlive() const {
    return _data.health>0;
}



void Particle::move(config_data *configData, int ocean_time_idx, Array1<double> &oceanTime, Array2<double> &mask,
                    Array2<double> &lonRad, Array2<double> &latRad, Array1<double> &sW, Array1<double> &depthIntervals,
                    Array2<double> &h, Array3<float> &zeta, Array4<float> &u, Array4<float> &v, Array4<float> &w,
                    Array4<float> &akt) {

    particle_data localParticleData;
    memcpy(&localParticleData, &_data, sizeof(particle_data));

    // Get the random flag
    bool random=configData->random;

    // Get the integration time (default 30s)
    double dti=configData->dti;

    // Get the time in seconds between two input ocean data (default 3600s, 1h)
    double deltat=configData->deltat;

    // Ask Angelo Riccio (default 86400)
    double tau0=configData->tau0;

    // Probability to survive (default 1.0e-4)
    double survprob=configData->survprob;

    // Reduction Coefficient (default 1)
    double crid=configData->crid;

    // Sedimentation velocity (m-1. default )
    double sv=configData->sv;

    // Sigma (deviation of particle distribution Baccaciola et Al. 1993.
    double sigma=configData->sigma;

    // Shore limit (positive depth: default 0.25)
    double shoreLimit=configData->shoreLimit;



    // Physical forcing interval
    int intervalDirection=configData->trackingDirection;
    int intervalNextIdx=ocean_time_idx+intervalDirection;
    double intervalStart=oceanTime(ocean_time_idx);
    double intervalEnd=intervalNextIdx >= 0 && intervalNextIdx < oceanTime.Nx()
                       ? oceanTime(intervalNextIdx) : intervalStart+intervalDirection*deltat;
    double intervalLength=std::abs(intervalEnd-intervalStart);
    double elapsed=0;
    if (!std::isnan(configData->restartCheckpoint)) {
        elapsed=NumericalHelpers::restartElapsed(intervalStart,intervalEnd,configData->restartCheckpoint);
        if (std::isnan(elapsed)) return;
    }

    // Get the number of the sigma levels
    size_t s_w = w.Ny();

    double kLowerLimit=-(int)s_w + 2;

    // Get the domain size in south-north number of rows
    size_t eta_rho = mask.Nx();

    // Get the domain size in west-east number of columns
    size_t xi_rho = mask.Ny();

    // Check if the particle jumped outside the water :-)
    if (localParticleData.k>0) {
        switch (configData->upperClosure) {

            case Config::CLOSURE_MODE_CONSTRAINT:
                // The particle must stay in the water
                localParticleData.k=0;
                break;

            case Config::CLOSURE_MODE_KILL:
                // Kill the particle
                localParticleData.health=-1;
                break;

            case Config::CLOSURE_MODE_REFLECTION:
                // Reflect the particle
                localParticleData.k=-localParticleData.k;
                break;
        }

    }

    // Check if the new k have to be limited by the seafoor
    if (localParticleData.k < kLowerLimit) {

        switch (configData->lowerClosure) {

            case Config::CLOSURE_MODE_CONSTRAINT:
                // Limit it on the bottom
                localParticleData.k = kLowerLimit ;
                break;

            case Config::CLOSURE_MODE_KILL:
                // The particle must stay in the water
                localParticleData.health=-1;
                break;

            case Config::CLOSURE_MODE_REFLECTION:
                // The particle must stay in the water
                localParticleData.k=2.0 * kLowerLimit - localParticleData.k;
                break;
        }

    }
    // For each integration interval
    while (elapsed < intervalLength) {

        double stepDt=std::min(dti,intervalLength-elapsed);
        double physicalTime=intervalStart + configData->trackingDirection * elapsed;
        double alpha=intervalLength == 0 ? 0 : elapsed/intervalLength;
        int nextOceanTimeIdx=intervalNextIdx;
        if (nextOceanTimeIdx < 0 || nextOceanTimeIdx >= oceanTime.Nx()) nextOceanTimeIdx=ocean_time_idx;

        auto zetaAt=[&](int j, int i) {
            return (float)NumericalHelpers::interpolateTime(zeta(ocean_time_idx,j,i),zeta(nextOceanTimeIdx,j,i),alpha);
        };
        auto uAt=[&](int k, int j, int i) {
            return (float)NumericalHelpers::interpolateTime(u(ocean_time_idx,k,j,i),u(nextOceanTimeIdx,k,j,i),alpha);
        };
        auto vAt=[&](int k, int j, int i) {
            return (float)NumericalHelpers::interpolateTime(v(ocean_time_idx,k,j,i),v(nextOceanTimeIdx,k,j,i),alpha);
        };
        auto wAt=[&](int k, int j, int i) {
            return (float)NumericalHelpers::interpolateTime(w(ocean_time_idx,k,j,i),w(nextOceanTimeIdx,k,j,i),alpha);
        };
        auto aktAt=[&](int k, int j, int i) {
            return (float)NumericalHelpers::interpolateTime(akt(ocean_time_idx,k,j,i),akt(nextOceanTimeIdx,k,j,i),alpha);
        };

#ifdef DEBUG
        LOG4CPLUS_DEBUG(logger,"time:" << physicalTime);
        LOG4CPLUS_DEBUG(logger, "k:" << localParticleData.k << " j:" << localParticleData.j << " i:" << localParticleData.i );
        LOG4CPLUS_DEBUG(logger, "age:" << localParticleData.age << " health:" << localParticleData.health << " time:" << localParticleData.time );
#endif

        // Check if the particle is not yet active
        if (configData->trackingDirection > 0 && localParticleData.time>physicalTime) {
            // The particle is not already active (already emitted, but not active)
            elapsed=std::min(intervalLength,localParticleData.time-intervalStart);
            continue;
        }

        // Check of the particle health is less than its probability to survive
        if (localParticleData.health<survprob) {
            // The particle is dead
            localParticleData.health=-1;
            // No reason to continue, exit the integration loop
            break;
        }

        /*
         * IMPORTANT: the k index is negative
         * in the interval [-s_w, 0]
         */

        // Get the integer part and the fraction part of particle k
        auto kI=(int)localParticleData.k; double kF=localParticleData.k-kI;

        // Get the integer part and the fraction part of particle j
        auto jI=(int)localParticleData.j; double jF=localParticleData.j-jI;

        // Get the integer part and the fraction part of particle i
        auto iI=(int)localParticleData.i; double iF=localParticleData.i-iI;

        // Check if the particle is out of the domain
        if (!NumericalHelpers::validInterpolationCell(jI, iI, eta_rho, xi_rho)) {

            // Set the particle health
            localParticleData.health=-1;

            // no reason to continue,  exit the integration loop
            break;
        }
	

	/*	
	// Check if the particle beached
	if (mask(jI,iI)<=0) {
	
		// Set the particle health
		localParticleData.health=-1;
	
		// no reason to continue,  exit the integration loop
		break;
	}*/


#ifdef DEBUG
        LOG4CPLUS_DEBUG(logger, "Alive");
#endif

        // The particle is alive!

        // Perform the bilinear interpolation (2D) in order to get
        // the zeta at the particle position.
        float z1 = zetaAt(jI, iI) * (1.0 - iF) * (1.0 - jF);
        float z2 = zetaAt(jI + 1, iI) * (1.0 - iF) * jF;
        float z3 = zetaAt(jI + 1, iI + 1) * iF * jF;
        float z4 = zetaAt(jI, iI + 1) * iF * (1.0 - jF);

        // The current zeta at the particle position
        float zz = z1 + z2 + z3 + z4;

        // Perform the bilinear interpolation (2D) in order to get
        // the h (depth) at the particle position.
        double h1=h(jI,    iI)    *(1.0-iF)  *(1.0-jF);
        double h2=h(jI+1,  iI)    *(1.0-iF)  *     jF;
        double h3=h(jI+1,  iI+1)  *     iF   *     jF;
        double h4=h(jI  ,  iI+1)  *     iF   *(1.0-jF);

        // The current h (depth) at the particle position
        double hh=h1+h2+h3+h4;

        // Calculate the corrected depth
        double hc=hh+zz;

#ifdef DEBUG
        LOG4CPLUS_DEBUG(logger, "zz:" << zz << " hh:" << hh << " hc:" << hc);
#endif

        /*
         * TO CHECK
        // Calculate the depth of the particle in meters
        //double particleDepth=-(hc*abs(sW(kI))+abs(kF*depthIntervals(kI)));
	
        // Perform the linear interpolation (1D) in order tho get the particle depth.
        float p1 = sW(kI) * kF;
        float p2 = sW(kI - 1) * (1.0 - kF);
        float pp = p1 - p2;

        double particleDepth = hc * pp ;
        //particleDepth += abs(kF*depthIntervals(kI));
        */

        // Check if the particle is landed
        if (hc <= shoreLimit) {
            switch (configData->horizontalClosure) {

                case Config::CLOSURE_MODE_CONSTRAINT:
                    break;

                case Config::CLOSURE_MODE_KILL:
                    localParticleData.health=-1;
                    break;

                case Config::CLOSURE_MODE_REFLECTION:
                    // Reflect the particle
                    /*
                     * TO CHECK
                     if (idetI < iI) {
                            idet = (double) iI + abs(localParticleData.i - idet);
                        } else if (idetI > iI) {
                            idet = (double) idetI - mod(idet, 1.0);
                        }
                        if (jdetI < jdet) {
                            jdet = (double) jdetI + abs(localParticleData.j - jdet);
                        } else if (jdetI > jI) {
                            jdet = (double) jdetI - mod(jdet, 1.0);
                        }
                     */
                    break;
            }
        }

        //TO CHECK
        // Check if the particle is still floating...
        //if (hc>shoreLimit) {

            // The particle is still floating
#ifdef DEBUG
            LOG4CPLUS_DEBUG(logger, "Floating: kI:" << kI << " jI:" << jI << " iI:" << iI);
#endif

            // Perform the bilinear interpolation (2D) in order to get
            // the u component of the current field in the particle position.
            float u1 = uAt(kI, jI, iI) * (1.0 - iF) * (1.0 - jF);
            float u2 = uAt(kI, jI + 1, iI) * (1.0 - iF) * jF;
            float u3 = uAt(kI, jI + 1, iI + 1) * iF * jF;
            float u4 = uAt(kI, jI, iI + 1) * iF * (1.0 - jF);

            // The current u component in the particle position
            float uu = u1 + u2 + u3 + u4;


#ifdef DEBUG
            LOG4CPLUS_DEBUG(logger, "uu:" << uu);
#endif

            // Perform the bilinear interpolation (2D) in order to get
            // the v component of the current field in the particle position.
            float v1 = vAt(kI, jI, iI) * (1.0 - iF) * (1.0 - jF);
            float v2 = vAt(kI, jI + 1, iI) * (1.0 - iF) * jF;
            float v3 = vAt(kI, jI + 1, iI + 1) * iF * jF;
            float v4 = vAt(kI, jI, iI + 1) * iF * (1.0 - jF);

            // The current v component in the particle position
            float vv = v1 + v2 + v3 + v4;

#ifdef DEBUG
            LOG4CPLUS_DEBUG(logger, "vv:" << vv );
#endif

            // 0,0,690,533

            // Perform the bilinear interpolation (3D) in order to get
            // the w component of the current field in the particle position.
            float w1 = wAt(kI, jI, iI) * (1.0 - iF) * (1.0 - jF) * (1.0 - kF);
            float w2 = wAt(kI, jI + 1, iI) * (1.0 - iF) * jF * (1.0 - kF);
            float w3 = wAt(kI, jI + 1, iI + 1) * iF * jF * (1.0 - kF);
            float w4 = wAt(kI, jI, iI + 1) * iF * (1.0 - jF) * (1.0 - kF);
            float w5 = wAt(kI - 1, jI, iI) * (1.0 - iF) * (1.0 - jF) * kF;
            float w6 = wAt(kI - 1, jI + 1, iI) * (1.0 - iF) * jF * kF;
            float w7 = wAt(kI - 1, jI + 1, iI + 1) * iF * jF * kF;
            float w8 = wAt(kI - 1, jI, iI + 1) * iF * (1.0 - jF) * kF;

            // The current w component in the particle position
            float ww = w1 + w2 + w3 + w4 + w5 + w6 + w7 + w8;

#ifdef DEBUG
            LOG4CPLUS_DEBUG(logger, "ww:" << ww);
#endif


            // Perform the bilinear interpolation (3D) in order to get
            // the akt in the particle position.
            float a1 = aktAt(kI, jI, iI) * (1.0 - iF) * (1.0 - jF) * (1.0 - kF);
            float a2 = aktAt(kI, jI + 1, iI) * (1.0 - iF) * jF * (1.0 - kF);
            float a3 = aktAt(kI, jI + 1, iI + 1) * iF * jF * (1.0 - kF);
            float a4 = aktAt(kI, jI, iI + 1) * iF * (1.0 - jF) * (1.0 - kF);
            float a5 = aktAt(kI - 1, jI, iI) * (1.0 - iF) * (1.0 - jF) * kF;
            float a6 = aktAt(kI - 1, jI + 1, iI) * (1.0 - iF) * jF * kF;
            float a7 = aktAt(kI - 1, jI + 1, iI + 1) * iF * jF * kF;;
            float a8 = aktAt(kI - 1, jI, iI + 1) * iF * (1.0 - jF) * kF;

            // The AKT at the particle position.
            float aa = a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8;

#ifdef DEBUG
            LOG4CPLUS_DEBUG(logger, "aa:" << aa);
#endif

            // Evaluate the particle leap (in meters along x,y,z) due to the current field (deterministic leap).
            double direction=configData->trackingDirection;
            double dxleap = direction * uu * stepDt;
            double dyleap = direction * vv * stepDt;
            double dzleap = direction * (sv + ww) * stepDt;


            double rxleap=0;
            double ryleap=0;
            double rzleap=0;


            if (random && (direction > 0 || configData->backwardDiffusion)) {
                // Calculation of sigma for the particle depth
                double sigmaDepth = sigma * (1 - localParticleData.k / kLowerLimit);

                // Generate the random leap from the physical interval and absolute substep
                std::int64_t intervalKey=static_cast<std::int64_t>(std::llround(std::min(intervalStart,intervalEnd)));
                std::uint64_t substep=static_cast<std::uint64_t>(std::floor(elapsed/dti));
                double stochasticScale=std::sqrt(stepDt/dti);
                rxleap= sigmaDepth * NumericalHelpers::normal(configData->randomSeed,localParticleData.id,
                                                               intervalKey,substep,0) * stochasticScale;
                ryleap= sigmaDepth * NumericalHelpers::normal(configData->randomSeed,localParticleData.id,
                                                               intervalKey,substep,1) * stochasticScale;
                rzleap= sigmaDepth * NumericalHelpers::normal(configData->randomSeed,localParticleData.id,
                                                               intervalKey,substep,2) * aa * crid * stochasticScale;

            }

            // Final leap in meters
            double xleap = dxleap + rxleap;
            double yleap = dyleap + ryleap;
            double zleap = dzleap + rzleap;

#ifdef DEBUG
            LOG4CPLUS_DEBUG(logger, "zleap:" << zleap << " yleap:" << yleap << " xleap:" << xleap );
#endif

            double dLat, dLon, dLatLon, ydist, xdist, zdist;

            // Calculate the distance in radiants of latitude between the grid cell where is
            // currently located the particle and the next one.
            dLat = (latRad(jI + 1, iI) - latRad(jI, iI));
            dLon = 0;

            // Grid cell size along latitude using the Haversine method
            // https://www.movable-type.co.uk/scripts/latlong.html
            dLatLon = pow(sin(0.5 * dLat), 2) +
                    pow(sin(0.5 * dLon), 2) *
                    cos(latRad(jI + 1, iI)) *
                    cos(latRad(jI, iI));

            // Size of a grid cell along latitude in meters
            ydist = 2.0 * atan2(pow(dLatLon, .5), pow(1.0 - dLatLon, .5)) * 6371000.0;

            // Calculate the distance in radiants of longitude between the grid cell where is
            // currently located the particle and the next one.
            dLat = 0;
            dLon = (lonRad(jI, iI + 1) - lonRad(jI, iI));

            // Grid cell size along latitude using the Haversine method
            // https://www.movable-type.co.uk/scripts/latlong.html
            dLatLon = pow(sin(0.5 * dLat), 2) +
                 pow(sin(0.5 * dLon), 2) *
                 cos(latRad(jI + 1, iI)) *
                 cos(latRad(jI, iI));

            // Size of a grid cell along latitude in meters
            xdist = 2.0 * atan2(pow(dLatLon, .5), pow(1.0 - dLatLon, .5)) * 6371000.0;

            // Vertical dimension of a grid cell in meters
            //zdist = hc * (depthIntervals(kI) * kF - depthIntervals(kI + 1) * (1 - kF)); //INTERPOLATION FORMULA
            //zdist = hc * (sW(kI) * kF - sW(kI - 1) * (1 - kF)); //OLD FORMULA
            zdist = hc * depthIntervals(kI);

#ifdef DEBUG
            LOG4CPLUS_DEBUG(logger, "hc:" << hc << " depthIntervals(" << kI <<"):" << depthIntervals(kI) << " zdist:" << zdist << " ydist:" << ydist << " xdist:" << xdist );
#endif

            // Check if the vertical leap is greather than the vertical dimension of the cell in meters
            if (abs(zleap) > zdist) {

                // Limit the leap to the vertical grid cell size, but in the direction of the leap
                zleap = sign(zdist, zleap);
            }

            // Calculate the new particle j candidate
            double jdet = localParticleData.j + yleap / ydist;

            // Calculate the new particle i candidate
            double idet = localParticleData.i + xleap / xdist;

            // Calculate the new particle k candidate
            double kdet = localParticleData.k + zleap / zdist;


#ifdef DEBUG
            LOG4CPLUS_DEBUG(logger, "kdet:" << kdet << " jdet:" << jdet << " idet:" << idet );
#endif

            // Check if the new k have to be limited by the seafloor
            if (kdet > 0.) {
                switch (configData->upperClosure) {

                    case Config::CLOSURE_MODE_CONSTRAINT:
                        // The particle must stay in the water
                        kdet=0;
                        break;

                    case Config::CLOSURE_MODE_KILL:
                        // The particle must stay in the water
                        localParticleData.health=-1;
                        break;

                    case Config::CLOSURE_MODE_REFLECTION:
                        // The particle must stay in the water
                        kdet = -kdet;
                        break;
                }
                    }

            // Check if the new k have to be limited by the sealfoor
            if (kdet < kLowerLimit) {
                switch (configData->lowerClosure) {

                    case Config::CLOSURE_MODE_CONSTRAINT:
                        // Limit it on the bottom
                        kdet = kLowerLimit ;
                        break;

                    case Config::CLOSURE_MODE_KILL:
                        // The particle must stay in the water
                        localParticleData.health=-1;
                        break;

                    case Config::CLOSURE_MODE_REFLECTION:
                        // The particle is reflected
                        kdet=2.0 * kLowerLimit - kdet;
                        break;
                }
            }

            // Reflect if crossed the coastline

            // Calculate the integer part of the j, i, k candidates
            int jdetI = (int) (jdet);
            int idetI = (int) (idet);
            int kdetI = (int) (kdet);

		    // Check if the candidate position is within the domain
            if (NumericalHelpers::validInterpolationCell(jdetI, idetI, eta_rho, xi_rho)) {
		
                // Check if the candidate new particle position is on land (cfr. shoreLimit)
                double idetF = idet-idetI;
                double jdetF = jdet-jdetI;
                double kdetF = kdet - kdetI;

                // Perform the bilinear interpolation (2D) in order to get
                // the zeta at the new particle position.
                z1 = zetaAt(jdetI, idetI) * (1.0 - idetF) * (1.0 - jdetF);
                z2 = zetaAt(jdetI + 1, idetI) * (1.0 - idetF) * jdetF;
                z3 = zetaAt(jdetI + 1, idetI + 1) * idetF * jdetF;
                z4 = zetaAt(jdetI, idetI + 1) * idetF * (1.0 - jdetF);

                // The new zeta at the particle position
                float zzdet = z1 + z2 + z3 + z4;

                // Perform the bilinear interpolation (2D) in order to get
                // the h (depth) at the new particle position.
                h1=h(jdetI,    idetI)    *(1.0-idetF)  *(1.0-jdetF);
                h2=h(jdetI+1,  idetI)    *(1.0-idetF)  *     jdetF;
                h3=h(jdetI+1,  idetI+1)  *     idetF   *     jdetF;
                h4=h(jdetI  ,  idetI+1)  *     idetF   *(1.0-jdetF);

                // The current h (depth) at the new particle position
                double hhdet = h1+h2+h3+h4;

                // Calculate the new corrected depth
                double hcdet = hhdet + zzdet;

		        /*
		         * TO CHECK
                // Perform the linear interpolation in order to get
                // the new particle depth
		        p1 = sW(kdetI) * kdetF;
		        p2 = sW(kdetI - 1) * (1.0 - kdetF);
                float ppdet = p1 - p2;

                double particleDepthdet = hcdet * ppdet ;
                */
		
                // Check if the new particle is landed
                if (hcdet <= shoreLimit) {
                    switch (configData->horizontalClosure) {

                                case Config::CLOSURE_MODE_CONSTRAINT:
                                    break;

                                case Config::CLOSURE_MODE_KILL:
                                    localParticleData.health=-1;
                                    break;

                                case Config::CLOSURE_MODE_REFLECTION:
                                    // Reflect the particle
                                    NumericalHelpers::reflectCell(localParticleData.i, iI, idet, idetI);
                                    NumericalHelpers::reflectCell(localParticleData.j, jI, jdet, jdetI);
                                    break;
                            }
                       }
		
                // Assign the new particle position
                localParticleData.i = idet;
                localParticleData.j = jdet;
                localParticleData.k = kdet;
           }

	//}


        // Check if the particle is still alive
        if (localParticleData.health>0) {

            // Update the particle age
            localParticleData.age = localParticleData.age + stepDt;

            // Decay the particle
            localParticleData.health = health0 * exp(-localParticleData.age / tau0);
        }

#ifdef DEBUG
        LOG4CPLUS_DEBUG(logger, "k:" << localParticleData.k << " j:" << localParticleData.j << " i:" << localParticleData.i );
        LOG4CPLUS_DEBUG(logger, "age:" << localParticleData.age << " health:" << localParticleData.health << " time:" << localParticleData.time );
        LOG4CPLUS_DEBUG(logger,"time:" << physicalTime);
#endif
        elapsed+=stepDt;
    }
    memcpy(&_data, &localParticleData, sizeof(particle_data));
}

double Particle::K() const  { return _data.k; }
double Particle::J() const { return _data.j; }
double Particle::I() const { return _data.i; }



// Returns -1 if a < 0 and 1 if a > 0
double Particle::sgn(double a) { return (a > 0) - (a < 0); }

// Computes the remainder of the division of a by p.
// https://gcc.gnu.org/onlinedocs/gfortran/MOD.html
double Particle::mod(double a, double p) { return a-p*(int)(a/p); }

// Returns the value of a with the sign of b.
// https://gcc.gnu.org/onlinedocs/gfortran/SIGN.html
double Particle::sign(double a, double b) { return abs(a)*sgn(b); }

std::string Particle::to_string() const {
    std::stringstream ss;
    ss << _data.id << " " << _data.k << " " << _data.j << " " << _data.i << " " << _data.health << " " << _data.age << " " << _data.time;
    return ss.str();
}

double Particle::Age() const {
    return _data.age;
}

double Particle::Health() const {
    return _data.health;
}

double Particle::Time() const {
    return _data.time;
}

std::uint64_t Particle::Id() const {
    return _data.id;
}
