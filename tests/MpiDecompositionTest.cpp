#include "../MpiHelpers.hpp"
#include "../NumericalHelpers.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

int main(int argc, char **argv) {
    MPI_Init(&argc,&argv);
    int worldSize=1,worldRank=0;
    MPI_Comm_size(MPI_COMM_WORLD,&worldSize);
    MPI_Comm_rank(MPI_COMM_WORLD,&worldRank);

    const int particleCount=11;
    std::vector<int> counts(worldSize),displacements(worldSize);
    for (int rank=0;rank<worldSize;rank++) {
        counts[rank]=(int)NumericalHelpers::partitionCount(particleCount,worldSize,rank);
        displacements[rank]=(int)NumericalHelpers::partitionOffset(particleCount,worldSize,rank);
    }
    std::vector<particle_data> input,parallelResult;
    if (worldRank==0) {
        for (int idx=0;idx<particleCount;idx++) {
            input.push_back({9007199254740993ULL+(std::uint64_t)idx,-.5,.25,.25,1,0,0});
        }
        parallelResult.resize(particleCount);
    }
    std::vector<particle_data> local(counts[worldRank]);
    MPI_Datatype particleType=MpiHelpers::particleDataType();
    MPI_Scatterv(input.data(),counts.data(),displacements.data(),particleType,
                 local.data(),counts[worldRank],particleType,0,MPI_COMM_WORLD);

    Array1<double> oceanTime(2); oceanTime(0)=0; oceanTime(1)=60;
    Array2<double> mask(2,2),lonRad(2,2),latRad(2,2),h(2,2);
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        mask(j,i)=1; lonRad(j,i)=i*0.017453292519943295;
        latRad(j,i)=j*0.017453292519943295; h(j,i)=100;
    }
    Array1<double> sW(3,-2),depthIntervals(3,-1);
    sW(-2)=-1; sW(-1)=-.5; sW(0)=0;
    depthIntervals(-1)=.5; depthIntervals(0)=.5; depthIntervals(1)=.5;
    Array3<float> zeta(2,2,2); zeta=0.0f;
    Array4<float> u(2,2,2,2,0,-1,0,0),v(2,2,2,2,0,-1,0,0);
    Array4<float> w(2,3,2,2,0,-2,0,0),akt(2,3,2,2,0,-2,0,0);
    u=0.0f; v=0.0f; w=0.0f; akt=0.0f;
    config_data config{};
    config.random=true; config.randomSeed=5489; config.deltat=60; config.dti=30;
    config.survprob=0; config.tau0=86400; config.crid=1; config.sigma=.1;
    config.shoreLimit=.25; config.upperClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.lowerClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.horizontalClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.trackingDirection=Config::TRACKING_FORWARD;
    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();

    for (particle_data &data:local) {
        Particle particle(data);
        particle.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
        data=particle.data();
    }
    MPI_Gatherv(local.data(),counts[worldRank],particleType,
                parallelResult.data(),counts.data(),displacements.data(),particleType,0,MPI_COMM_WORLD);

    if (worldRank==0) {
        for (int idx=0;idx<particleCount;idx++) {
            Particle particle(input[idx]);
            particle.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
            particle_data serial=particle.data();
            assert(parallelResult[idx].id==serial.id);
            assert(parallelResult[idx].i==serial.i && parallelResult[idx].j==serial.j);
            assert(parallelResult[idx].k==serial.k && parallelResult[idx].health==serial.health);
            assert(parallelResult[idx].age==serial.age && parallelResult[idx].time==serial.time);
        }
    }
    MPI_Type_free(&particleType);
    MPI_Finalize();
}
