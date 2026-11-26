// Verification of the forcing kept on the GPU across windows (docs/parallelism.md#cuda-device-forcing).
//
// Wacomm::uploadForcing must leave on the device exactly the host arrays of the current window,
// whether the boundary record shared with the previous window is uploaded, moved inside the
// resident arrays, or carried into grown arrays. Every field varies with physical time, depth,
// and position, so a record copied to the wrong slot or a stale record changes the comparison.
// Windows sharing a time share bit-identical records, as appendBoundaryRecord guarantees.
// The sequences cover forward and backward tracking, growth to three and four records,
// a shrink, a broken window chain, and a boundary time that does not match.

#include "../Wacomm.hpp"
#include "../Config.hpp"

#include <cuda_runtime_api.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>
#include <log4cplus/initializer.h>

using json = nlohmann::json;

namespace {

class FixtureAdapter: public OceanModelAdapter {
public:
    void process() override {}
};

constexpr int NJ=3, NI=4;

// Field value from physical time (h), level, and cell: identical for every window holding that time
float value(int field, double hour, int k, int j, int i) {
    return (float) (field*1000+hour*10+k+0.1*j+0.01*i);
}

std::shared_ptr<FixtureAdapter> window(const std::vector<double>& hours) {
    auto adapter=std::make_shared<FixtureAdapter>();
    int records=(int) hours.size();
    adapter->OceanTime().Allocate(records);
    for (int t=0;t<records;t++) adapter->OceanTime()(t)=hours[t]*3600;
    adapter->SRho().Allocate(2,-1); adapter->SRho()(-1)=-.75; adapter->SRho()(0)=-.25;
    adapter->SW().Allocate(3,-2); adapter->SW()(-2)=-1; adapter->SW()(-1)=-.5; adapter->SW()(0)=0;
    adapter->DepthIntervals().Allocate(3,-1);
    for (int k=-1;k<=1;k++) adapter->DepthIntervals()(k)=.5;
    for (auto grid: {&adapter->Mask(),&adapter->LonRad(),&adapter->LatRad(),&adapter->H()}) grid->Allocate(NJ,NI);
    for (int j=0;j<NJ;j++) for (int i=0;i<NI;i++) {
        adapter->Mask()(j,i)=1; adapter->LonRad()(j,i)=.24+i*1e-4;
        adapter->LatRad()(j,i)=.71+j*1e-4; adapter->H()(j,i)=100+j+i;
    }
    adapter->Zeta().Allocate(records,NJ,NI);
    adapter->U().Allocate(records,2,NJ,NI,0,-1,0,0);
    adapter->V().Allocate(records,2,NJ,NI,0,-1,0,0);
    adapter->W().Allocate(records,3,NJ,NI,0,-2,0,0);
    adapter->AKT().Allocate(records,3,NJ,NI,0,-2,0,0);
    for (int t=0;t<records;t++) for (int j=0;j<NJ;j++) for (int i=0;i<NI;i++) {
        adapter->Zeta()(t,j,i)=value(0,hours[t],0,j,i);
        for (int k=-1;k<=0;k++) {
            adapter->U()(t,k,j,i)=value(1,hours[t],k,j,i);
            adapter->V()(t,k,j,i)=value(2,hours[t],k,j,i);
        }
        for (int k=-2;k<=0;k++) {
            adapter->W()(t,k,j,i)=value(3,hours[t],k,j,i);
            adapter->AKT()(t,k,j,i)=value(4,hours[t],k,j,i);
        }
    }
    return adapter;
}

template<class T>
bool sameOnDevice(const T *device, const T *host, size_t count) {
    std::vector<T> copy(count);
    if (cudaMemcpy(copy.data(),device,count*sizeof(T),cudaMemcpyDeviceToHost)!=cudaSuccess) return false;
    return std::memcmp(copy.data(),host,count*sizeof(T))==0;
}

struct Step {
    std::vector<double> hours;  // record times of the window (h)
    bool chained;               // the previous window's boundary record came from this window
    bool reused;                // expected reuse of the resident boundary record
};

void run(const std::string& direction, const std::vector<Step>& steps) {
    const std::string fileName="cuda-device-forcing-"+direction+".json";
    std::ofstream(fileName) << json({{"io",{{"ocean_model","WACOMM"}}},
                                     {"tracking",{{"direction",direction}}}}).dump();
    auto config=std::make_shared<Config>(fileName);
    auto sources=std::make_shared<Sources>();
    auto particles=std::make_shared<Particles>();
    auto device=std::make_shared<DeviceForcing>();

    for (const Step& step: steps) {
        auto adapter=window(step.hours);
        device->reuseBoundary=step.chained;
        Wacomm wacomm(config,adapter,sources,particles,nullptr,nullptr,nullptr,device);
        wacomm.uploadForcing(1);
        assert(device->reuseBoundary==step.reused);
        assert(device->records==step.hours.size());
        assert(device->capacity>=device->records);

        size_t records=step.hours.size(), cells=NJ*NI;
        const WacommVariables& d=device->gpus[0];
        assert(sameOnDevice(d.oceanTimeDevice,(double *) adapter->OceanTime(),records));
        assert(sameOnDevice(d.depthIntervalsDevice,(double *) adapter->DepthIntervals(),3));
        assert(sameOnDevice(d.lonRadDevice,(double *) adapter->LonRad(),cells));
        assert(sameOnDevice(d.latRadDevice,(double *) adapter->LatRad(),cells));
        assert(sameOnDevice(d.maskDevice,(double *) adapter->Mask(),cells));
        assert(sameOnDevice(d.hDevice,(double *) adapter->H(),cells));
        assert(sameOnDevice(d.zetaDevice,(float *) adapter->Zeta(),records*cells));
        assert(sameOnDevice(d.uDevice,(float *) adapter->U(),records*2*cells));
        assert(sameOnDevice(d.vDevice,(float *) adapter->V(),records*2*cells));
        assert(sameOnDevice(d.wDevice,(float *) adapter->W(),records*3*cells));
        assert(sameOnDevice(d.aktDevice,(float *) adapter->AKT(),records*3*cells));
        assert(d.windU10Device==nullptr && d.stokesUDevice==nullptr);
    }
}

}

int main() {
    // log4cplus must shut down before main returns; MSVC otherwise deadlocks
    // at exit while destroying its thread pool.
    log4cplus::Initializer initializer;
    int devices=0;
    if (cudaGetDeviceCount(&devices)!=cudaSuccess || devices==0) return 77;

    // Forward: the previous window's last record is this window's first
    run("forward",{
        {{0,1},false,false},
        {{1,2},true,true},          // moved in place
        {{2,3,4},true,true},        // carried into grown arrays
        {{4,5},true,true},          // shrink inside the capacity
        {{7,8},false,false},        // broken chain: everything uploaded
        {{9,10},true,false}});      // chained but the boundary time differs

    // Backward: the previous window's first record is this window's last
    run("backward",{
        {{8,9},false,false},
        {{7,8},true,true},
        {{4,5,6,7},true,true},
        {{3,4},true,true},
        {{0,1},false,false}});

    std::printf("CUDA device forcing: forward and backward windows match the host arrays\n");
    return 0;
}
