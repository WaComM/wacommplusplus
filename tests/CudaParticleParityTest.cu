#include "../Particle.hpp"
#include "../cuda/kernel.h"

#include <cuda_runtime_api.h>

#include <cassert>
#include <cmath>
#include <limits>

namespace {

template<class T>
T *copyToDevice(const T *source, std::size_t count) {
    T *device=nullptr;
    if (cudaMalloc(&device,count*sizeof(T))!=cudaSuccess) return nullptr;
    if (cudaMemcpy(device,source,count*sizeof(T),cudaMemcpyHostToDevice)!=cudaSuccess) {
        cudaFree(device);
        return nullptr;
    }
    return device;
}

bool close(double left, double right, double tolerance) {
    return std::abs(left-right)<=tolerance;
}

}

int main() {
    int devices=0;
    cudaError_t deviceStatus=cudaGetDeviceCount(&devices);
    if (deviceStatus!=cudaSuccess || devices==0) return 77;

    double oceanTime[2]={0,65};
    double mask[4]={1,1,1,1};
    double lonRad[4]={0,.017453292519943295,0,.017453292519943295};
    double latRad[4]={0,0,.017453292519943295,.017453292519943295};
    double depthIntervals[3]={.5,.5,.5};
    double h[4]={100,100,100,100};
    float zeta[8]={};
    float u[16]={},v[16]={},w[24]={},akt[24]={};
    for (int index=8;index<16;index++) u[index]=.25f;
    for (float &value:akt) value=.01f;

    Array1<double> cpuTime(2); cpuTime(0)=oceanTime[0]; cpuTime(1)=oceanTime[1];
    Array2<double> cpuMask(2,2),cpuLon(2,2),cpuLat(2,2),cpuH(2,2);
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        int index=j*2+i;
        cpuMask(j,i)=mask[index]; cpuLon(j,i)=lonRad[index];
        cpuLat(j,i)=latRad[index]; cpuH(j,i)=h[index];
    }
    Array1<double> cpuSW(3,-2),cpuDepth(3,-1);
    cpuSW(-2)=-1; cpuSW(-1)=-.5; cpuSW(0)=0;
    cpuDepth(-1)=.5; cpuDepth(0)=.5; cpuDepth(1)=.5;
    Array3<float> cpuZeta(2,2,2); cpuZeta=0.0f;
    Array4<float> cpuU(2,2,2,2,0,-1,0,0),cpuV(2,2,2,2,0,-1,0,0);
    Array4<float> cpuW(2,3,2,2,0,-2,0,0),cpuAkt(2,3,2,2,0,-2,0,0);
    cpuU=0.0f; cpuV=0.0f; cpuW=0.0f; cpuAkt=.01f;
    for (int k=-1;k<=0;k++) for (int j=0;j<2;j++) for (int i=0;i<2;i++) cpuU(1,k,j,i)=.25f;

    config_data config{};
    config.random=false; config.randomSeed=5489; config.deltat=3600; config.dti=30;
    config.survprob=0; config.tau0=86400; config.crid=1; config.sigma=.1;
    config.shoreLimit=.25; config.upperClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.lowerClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.horizontalClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.trackingDirection=Config::TRACKING_FORWARD;
    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();

    for (int stochastic=0;stochastic<2;stochastic++) {
        config.random=stochastic!=0;
        Particle cpu(9007199254740993ULL,-.5,.25,.25,0);
        cpu.move(&config,0,cpuTime,cpuMask,cpuLon,cpuLat,cpuSW,cpuDepth,cpuH,cpuZeta,
                 cpuU,cpuV,cpuW,cpuAkt);
        particle_data result=cpu.data();

        particle_data initial{9007199254740993ULL,-.5,.25,.25,1,0,0};
        config_data *deviceConfig=copyToDevice(&config,1);
        particle_data *deviceParticle=copyToDevice(&initial,1);
        double *deviceTime=copyToDevice(oceanTime,2),*deviceMask=copyToDevice(mask,4);
        double *deviceLon=copyToDevice(lonRad,4),*deviceLat=copyToDevice(latRad,4);
        double *deviceDepth=copyToDevice(depthIntervals,3),*deviceH=copyToDevice(h,4);
        float *deviceZeta=copyToDevice(zeta,8),*deviceU=copyToDevice(u,16);
        float *deviceV=copyToDevice(v,16),*deviceW=copyToDevice(w,24);
        float *deviceAkt=copyToDevice(akt,24);
        assert(deviceConfig && deviceParticle && deviceTime && deviceMask && deviceLon && deviceLat &&
               deviceDepth && deviceH && deviceZeta && deviceU && deviceV && deviceW && deviceAkt);

        assert(cudaMoveParticle(deviceConfig,deviceParticle,0,2,3,2,2,2,deviceTime,deviceMask,
                                deviceLon,deviceLat,deviceDepth,deviceH,deviceZeta,deviceU,deviceV,
                                deviceW,deviceAkt,1,1,1)==cudaSuccess);
        assert(cudaDeviceSynchronize()==cudaSuccess);
        assert(cudaMemcpy(&result,deviceParticle,sizeof(result),cudaMemcpyDeviceToHost)==cudaSuccess);

        double tolerance=stochastic ? 1e-9 : 1e-12;
        assert(result.id==cpu.Id());
        assert(close(result.i,cpu.I(),tolerance) && close(result.j,cpu.J(),tolerance));
        assert(close(result.k,cpu.K(),tolerance) && close(result.health,cpu.Health(),tolerance));
        assert(close(result.age,cpu.Age(),tolerance) && close(result.time,cpu.Time(),tolerance));

        cudaFree(deviceConfig); cudaFree(deviceParticle); cudaFree(deviceTime); cudaFree(deviceMask);
        cudaFree(deviceLon); cudaFree(deviceLat); cudaFree(deviceDepth); cudaFree(deviceH);
        cudaFree(deviceZeta); cudaFree(deviceU); cudaFree(deviceV); cudaFree(deviceW); cudaFree(deviceAkt);
    }
}
