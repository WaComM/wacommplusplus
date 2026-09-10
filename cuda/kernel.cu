//
// Created by Ciro Giuseppe De Vita and Gennaro Mellone on 24/12/20.
//

#include <cuda.h>
#include <cuda_runtime_api.h>

#include "../Config.hpp"
#include "../Particle.hpp"
#include "kernel.h"

namespace {

__device__ double sign(double value, double direction) { return fabs(value)*((direction>0)-(direction<0)); }
__device__ bool validCell(int j, int i, int eta, int xi) { return j>=0 && i>=0 && j<eta-1 && i<xi-1; }

__device__ unsigned long long mix(unsigned long long value) {
    value+=0x9e3779b97f4a7c15ULL;
    value=(value^(value>>30U))*0xbf58476d1ce4e5b9ULL;
    value=(value^(value>>27U))*0x94d049bb133111ebULL;
    return value^(value>>31U);
}

__device__ double uniform(unsigned long long seed, unsigned long long particle, long long interval,
                          unsigned long long substep, unsigned long long component) {
    unsigned long long key=mix(seed)^mix(particle)^mix((unsigned long long)interval);
    key^=mix(substep)^mix(component);
    return (mix(key)>>11U)*0x1.0p-53;
}

__device__ double normal(unsigned long long seed, unsigned long long particle, long long interval,
                         unsigned long long substep, unsigned long long component) {
    double u1=fmax(uniform(seed,particle,interval,substep,component*2),2.2250738585072014e-308);
    double u2=uniform(seed,particle,interval,substep,component*2+1);
    return sqrt(-2.0*log(u1))*cos(6.28318530717958647692*u2);
}

__device__ double forcingErrorNormal(unsigned long long seed,unsigned long long particle,long long interval,
                                     unsigned long long substep,unsigned long long component,double longitude,
                                     double latitude,double physicalTime,double spatialScale,double temporalScale) {
    if (spatialScale<=0 && temporalScale<=0) return normal(seed,particle,interval,substep,component);
    unsigned long long spatialKey=particle;
    if (spatialScale>0) {
        const double earthRadius=6371000.0;
        long long x=(long long)floor(earthRadius*longitude*cos(latitude)/spatialScale);
        long long y=(long long)floor(earthRadius*latitude/spatialScale);
        spatialKey=mix((unsigned long long)x)^mix((unsigned long long)y);
    }
    long long timeKey=interval;
    unsigned long long sampleKey=substep;
    if (temporalScale>0) { timeKey=(long long)floor(physicalTime/temporalScale); sampleKey=0; }
    return normal(seed,spatialKey,timeKey,sampleKey,component);
}

__device__ size_t index3(int t, int j, int i, int eta, int xi) {
    return ((size_t)t*eta+j)*xi+i;
}

__device__ size_t index4(int t, int k, int levels, int j, int i, int eta, int xi) {
    return (((size_t)t*levels+(k+levels-1))*eta+j)*xi+i;
}

__device__ float at(float *field, int t0, int t1, int j, int i, int eta, int xi, double alpha) {
    float v0=field[index3(t0,j,i,eta,xi)],v1=field[index3(t1,j,i,eta,xi)];
    return (float)((1.0-alpha)*v0+alpha*v1);
}

__device__ float at(float *field, int t0, int t1, int k, int levels, int j, int i,
                    int eta, int xi, double alpha) {
    float v0=field[index4(t0,k,levels,j,i,eta,xi)],v1=field[index4(t1,k,levels,j,i,eta,xi)];
    return (float)((1.0-alpha)*v0+alpha*v1);
}

__device__ double bilinear(double *field, int j, int i, double jf, double ifrac, int xi) {
    return field[j*xi+i]*(1.0-ifrac)*(1.0-jf)+field[(j+1)*xi+i]*(1.0-ifrac)*jf+
           field[(j+1)*xi+i+1]*ifrac*jf+field[j*xi+i+1]*ifrac*(1.0-jf);
}

__device__ float bilinear(float *field, int t0, int t1, int j, int i, double jf, double ifrac,
                          int eta, int xi, double alpha) {
    return at(field,t0,t1,j,i,eta,xi,alpha)*(1.0-ifrac)*(1.0-jf)+
           at(field,t0,t1,j+1,i,eta,xi,alpha)*(1.0-ifrac)*jf+
           at(field,t0,t1,j+1,i+1,eta,xi,alpha)*ifrac*jf+
           at(field,t0,t1,j,i+1,eta,xi,alpha)*ifrac*(1.0-jf);
}

__device__ float bilinear(float *field, int t0, int t1, int k, int levels, int j, int i,
                          double jf, double ifrac, int eta, int xi, double alpha) {
    return at(field,t0,t1,k,levels,j,i,eta,xi,alpha)*(1.0-ifrac)*(1.0-jf)+
           at(field,t0,t1,k,levels,j+1,i,eta,xi,alpha)*(1.0-ifrac)*jf+
           at(field,t0,t1,k,levels,j+1,i+1,eta,xi,alpha)*ifrac*jf+
           at(field,t0,t1,k,levels,j,i+1,eta,xi,alpha)*ifrac*(1.0-jf);
}

__device__ void reflect(double oldCoordinate, int oldCell, double &candidate, int candidateCell) {
    if (candidateCell<oldCell) candidate=oldCell+fabs(oldCoordinate-candidate);
    else if (candidateCell>oldCell) candidate=candidateCell-fmod(candidate,1.0);
}

__device__ double reflectDomain(double coordinate, double maximum) {
    if (maximum<=0) return 0;
    double reflected=fmod(coordinate,2.0*maximum);
    if (reflected<0) reflected+=2.0*maximum;
    if (reflected>=maximum) reflected=2.0*maximum-reflected;
    if (reflected>=maximum) reflected=nextafter(maximum,0.0);
    return reflected;
}

__device__ double distance(double lat0, double lon0, double lat1, double lon1) {
    double dLat=lat1-lat0,dLon=lon1-lon0;
    double value=sin(.5*dLat)*sin(.5*dLat)+sin(.5*dLon)*sin(.5*dLon)*cos(lat1)*cos(lat0);
    return 2.0*atan2(sqrt(value),sqrt(1.0-value))*6371000.0;
}

}

__global__ void move(config_data *config, particle_data *particles, int timeIndex, int oceanTime,
                     int sW, int sRho, int eta, int xi, double *times, double *mask, double *lonRad,
                     double *latRad, double *depthIntervals, double *h, float *zeta, float *u,
                     float *v, float *w, float *akt, float *windU10, float *windV10,
                     float *stokesU, float *stokesV, int particleCount) {
    int idx=threadIdx.x+blockIdx.x*blockDim.x;
    if (idx>=particleCount) return;
    particle_data particle=particles[idx];
    int direction=config->trackingDirection,nextTime=timeIndex+direction;
    double intervalStart=times[timeIndex];
    double intervalEnd=nextTime>=0 && nextTime<oceanTime ? times[nextTime] : intervalStart+direction*config->deltat;
    double intervalLength=fabs(intervalEnd-intervalStart),elapsed=0;
    if (!isnan(config->restartCheckpoint)) {
        if ((direction>0 && intervalEnd<=config->restartCheckpoint) ||
            (direction<0 && intervalEnd>=config->restartCheckpoint)) return;
        elapsed=fmin(intervalLength,fmax(0.0,direction*(config->restartCheckpoint-intervalStart)));
    }
    if (nextTime<0 || nextTime>=oceanTime) nextTime=timeIndex;
    double lowerLimit=-sW+2;

    if (particle.k>0) {
        if (config->upperClosure==Config::CLOSURE_MODE_CONSTRAINT) particle.k=0;
        else if (config->upperClosure==Config::CLOSURE_MODE_KILL) particle.health=-1;
        else if (config->upperClosure==Config::CLOSURE_MODE_REFLECTION) particle.k=-particle.k;
    }
    if (particle.k<lowerLimit) {
        if (config->lowerClosure==Config::CLOSURE_MODE_CONSTRAINT) particle.k=lowerLimit;
        else if (config->lowerClosure==Config::CLOSURE_MODE_KILL) particle.health=-1;
        else if (config->lowerClosure==Config::CLOSURE_MODE_REFLECTION) particle.k=2.0*lowerLimit-particle.k;
    }

    while (elapsed<intervalLength) {
        double stepDt=fmin(config->dti,intervalLength-elapsed);
        double physicalTime=intervalStart+direction*elapsed;
        if (direction>0 && particle.time>physicalTime) {
            elapsed=fmin(intervalLength,particle.time-intervalStart);
            continue;
        }
        if (particle.health<config->survprob) { particle.health=-1; break; }

        int kI=(int)ceil(particle.k),jI=(int)floor(particle.j),iI=(int)floor(particle.i);
        double kF=ceil(particle.k)-particle.k,jF=particle.j-jI,iF=particle.i-iI;
        if (!validCell(jI,iI,eta,xi) || kI>0 || kI<=-sW+1) { particle.health=-1; break; }
        double alpha=intervalLength==0 ? 0 : (elapsed+.5*stepDt)/intervalLength;
        float zz=bilinear(zeta,timeIndex,nextTime,jI,iI,jF,iF,eta,xi,alpha);
        double hc=bilinear(h,jI,iI,jF,iF,xi)+zz;
        if (hc<=config->shoreLimit && config->horizontalClosure==Config::CLOSURE_MODE_KILL) {
            particle.health=-1;
            break;
        }

        float uu=bilinear(u,timeIndex,nextTime,kI,sRho,jI,iI,jF,iF,eta,xi,alpha);
        float vv=bilinear(v,timeIndex,nextTime,kI,sRho,jI,iI,jF,iF,eta,xi,alpha);
        if (config->driftModel==1 &&
            particle.driftObjectType!=(unsigned short)DriftObjectType::PASSIVE) {
            LeewayCoefficients coefficients=driftObjectCoefficients((DriftObjectType)particle.driftObjectType);
            long long uncertaintyInterval=llround(fmin(intervalStart,intervalEnd));
            unsigned long long uncertaintySubstep=(unsigned long long)floor(elapsed/config->dti);
            double windU=windU10 ? bilinear(windU10,timeIndex,nextTime,jI,iI,jF,iF,eta,xi,alpha) : config->windU10;
            double windV=windV10 ? bilinear(windV10,timeIndex,nextTime,jI,iI,jF,iF,eta,xi,alpha) : config->windV10;
            if (config->windErrorStdDev>0) {
                double longitude=bilinear(lonRad,jI,iI,jF,iF,xi);
                double latitude=bilinear(latRad,jI,iI,jF,iF,xi);
                double first=forcingErrorNormal(config->randomSeed,particle.id,uncertaintyInterval,
                        uncertaintySubstep,LEEWAY_WIND_ERROR_U_COMPONENT,longitude,latitude,
                        physicalTime+.5*direction*stepDt,config->windErrorSpatialScale,config->windErrorTemporalScale);
                double second=forcingErrorNormal(config->randomSeed,particle.id,uncertaintyInterval,
                        uncertaintySubstep,LEEWAY_WIND_ERROR_V_COMPONENT,longitude,latitude,
                        physicalTime+.5*direction*stepDt,config->windErrorSpatialScale,config->windErrorTemporalScale);
                windU+=config->windErrorStdDev*first;
                windV+=config->windErrorStdDev*correlatedNormal(first,second,
                                                               config->windErrorComponentCorrelation);
            }
            double downwindNormal=0,crosswindNormal=0;
            if (config->leewayCoefficientEnsemble) {
                downwindNormal=normal(config->randomSeed,particle.id,LEEWAY_ENSEMBLE_RANDOM_INTERVAL,0,0);
                crosswindNormal=normal(config->randomSeed,particle.id,LEEWAY_ENSEMBLE_RANDOM_INTERVAL,0,1);
                crosswindNormal=correlatedNormal(downwindNormal,crosswindNormal,
                                                 config->leewayResidualCorrelation);
            }
            DriftVelocity leeway=computeLeeway(coefficients,windU,windV,
                                                (DriftSide)particle.driftSide,downwindNormal,crosswindNormal);
            uu+=(float)leeway.u;
            vv+=(float)leeway.v;
            if (stokesU && stokesV) {
                uu+=bilinear(stokesU,timeIndex,nextTime,jI,iI,jF,iF,eta,xi,alpha);
                vv+=bilinear(stokesV,timeIndex,nextTime,jI,iI,jF,iF,eta,xi,alpha);
            }
        }
        double ww=bilinear(w,timeIndex,nextTime,kI,sW,jI,iI,jF,iF,eta,xi,alpha)*(1.0-kF)+
                  bilinear(w,timeIndex,nextTime,kI-1,sW,jI,iI,jF,iF,eta,xi,alpha)*kF;
        double aa=bilinear(akt,timeIndex,nextTime,kI,sW,jI,iI,jF,iF,eta,xi,alpha)*(1.0-kF)+
                  bilinear(akt,timeIndex,nextTime,kI-1,sW,jI,iI,jF,iF,eta,xi,alpha)*kF;
        double xleap=direction*uu*stepDt,yleap=direction*vv*stepDt;
        double zleap=direction*(config->sv+ww)*stepDt;
        if (config->random && (direction>0 || config->backwardDiffusion)) {
            long long intervalKey=llround(fmin(intervalStart,intervalEnd));
            unsigned long long substep=(unsigned long long)floor(elapsed/config->dti);
            double scale=sqrt(stepDt/config->dti),sigmaDepth=config->sigma*(1-particle.k/lowerLimit);
            xleap+=sigmaDepth*normal(config->randomSeed,particle.id,intervalKey,substep,0)*scale;
            yleap+=sigmaDepth*normal(config->randomSeed,particle.id,intervalKey,substep,1)*scale;
            zleap+=sigmaDepth*normal(config->randomSeed,particle.id,intervalKey,substep,2)*aa*config->crid*scale;
        }

        double ydist=distance(latRad[jI*xi+iI],lonRad[jI*xi+iI],latRad[(jI+1)*xi+iI],lonRad[(jI+1)*xi+iI]);
        double xdist=distance(latRad[jI*xi+iI],lonRad[jI*xi+iI],latRad[jI*xi+iI+1],lonRad[jI*xi+iI+1]);
        double zdist=hc*depthIntervals[kI+sW-2];
        if (fabs(zleap)>zdist) zleap=sign(zdist,zleap);
        double jdet=particle.j+yleap/ydist,idet=particle.i+xleap/xdist,kdet=particle.k+zleap/zdist;
        if (kdet>0) {
            if (config->upperClosure==Config::CLOSURE_MODE_CONSTRAINT) kdet=0;
            else if (config->upperClosure==Config::CLOSURE_MODE_KILL) particle.health=-1;
            else if (config->upperClosure==Config::CLOSURE_MODE_REFLECTION) kdet=-kdet;
        }
        if (kdet<lowerLimit) {
            if (config->lowerClosure==Config::CLOSURE_MODE_CONSTRAINT) kdet=lowerLimit;
            else if (config->lowerClosure==Config::CLOSURE_MODE_KILL) particle.health=-1;
            else if (config->lowerClosure==Config::CLOSURE_MODE_REFLECTION) kdet=2.0*lowerLimit-kdet;
        }

        int jdetI=(int)floor(jdet),idetI=(int)floor(idet);
        if (validCell(jdetI,idetI,eta,xi)) {
            double jdetF=jdet-jdetI,idetF=idet-idetI;
            double hcdet=bilinear(h,jdetI,idetI,jdetF,idetF,xi)+
                         bilinear(zeta,timeIndex,nextTime,jdetI,idetI,jdetF,idetF,eta,xi,alpha);
            if (hcdet<=config->shoreLimit) {
                if (config->horizontalClosure==Config::CLOSURE_MODE_CONSTRAINT) { idet=particle.i; jdet=particle.j; }
                else if (config->horizontalClosure==Config::CLOSURE_MODE_KILL) particle.health=-1;
                else if (config->horizontalClosure==Config::CLOSURE_MODE_REFLECTION) {
                    reflect(particle.i,iI,idet,idetI);
                    reflect(particle.j,jI,jdet,jdetI);
                }
            }
            particle.i=idet; particle.j=jdet; particle.k=kdet;
        } else {
            if (config->horizontalClosure==Config::CLOSURE_MODE_CONSTRAINT) particle.k=kdet;
            else if (config->horizontalClosure==Config::CLOSURE_MODE_KILL) particle.health=-1;
            else if (config->horizontalClosure==Config::CLOSURE_MODE_REFLECTION) {
                particle.i=reflectDomain(idet,xi-1);
                particle.j=reflectDomain(jdet,eta-1);
                particle.k=kdet;
            }
        }
        if (particle.health>0) {
            particle.age+=stepDt; particle.health=exp(-particle.age/config->tau0);
            if (config->leewayJibeProbabilityHourly>0 &&
                particle.driftObjectType!=(unsigned short)DriftObjectType::PASSIVE) {
                long long intervalKey=llround(fmin(intervalStart,intervalEnd));
                unsigned long long substep=(unsigned long long)floor(elapsed/config->dti);
                double probability=jibeStepProbability(config->leewayJibeProbabilityHourly,stepDt);
                if (uniform(config->randomSeed,particle.id,intervalKey,substep,LEEWAY_JIBE_RANDOM_COMPONENT)<probability)
                    particle.driftSide=-particle.driftSide;
            }
        }
        elapsed+=stepDt;
    }
    particles[idx]=particle;
}

cudaError_t cudaMoveParticle(config_data *config, particle_data *particles, int timeIndex, int oceanTime,
                             int sW, int sRho, int eta, int xi, double *times, double *mask,
                             double *lonRad, double *latRad, double *depthIntervals, double *h,
                             float *zeta, float *u, float *v, float *w, float *akt,
                             float *windU10, float *windV10, float *stokesU, float *stokesV, int particleCount,
                             int numThread, int numGPU) {
    (void)mask; (void)numThread; (void)numGPU;
    dim3 threads=512;
    dim3 blocks=particleCount/threads.x+((particleCount%threads.x)==0 ? 0 : 1);
    move<<<blocks,threads>>>(config,particles,timeIndex,oceanTime,sW,sRho,eta,xi,times,mask,lonRad,latRad,
                            depthIntervals,h,zeta,u,v,w,akt,windU10,windV10,stokesU,stokesV,particleCount);
    return cudaGetLastError();
}
