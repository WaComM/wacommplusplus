#ifndef WACOMMPLUSPLUS_ADAPTERRESTARTEQUIVALENCE_HPP
#define WACOMMPLUSPLUS_ADAPTERRESTARTEQUIVALENCE_HPP

#include "../NumericalHelpers.hpp"
#include "../OceanModelAdapter.hpp"
#include "../Particle.hpp"

#include <cassert>
#include <cmath>
#include <limits>

inline void assertAdapterRestartEquivalence(OceanModelAdapter &adapter) {
    assert(adapter.OceanTime().Nx()>=2);
    int eta=(int)adapter.Mask().Nx(),xi=(int)adapter.Mask().Ny();
    int sRho=(int)adapter.SRho().Nx(),sW=(int)adapter.SW().Nx();
    double start=adapter.OceanTime()(0),end=adapter.OceanTime()(1),checkpoint=.5*(start+end);

    Array1<double> segmentTime(2); segmentTime(0)=start; segmentTime(1)=checkpoint;
    Array3<float> segmentZeta(2,eta,xi);
    Array4<float> segmentU(2,sRho,eta,xi,0,-sRho+1,0,0);
    Array4<float> segmentV(2,sRho,eta,xi,0,-sRho+1,0,0);
    Array4<float> segmentW(2,sW,eta,xi,0,-sW+1,0,0);
    Array4<float> segmentAkt(2,sW,eta,xi,0,-sW+1,0,0);
    for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
        segmentZeta(0,j,i)=adapter.Zeta()(0,j,i);
        segmentZeta(1,j,i)=(float)NumericalHelpers::interpolateTime(adapter.Zeta()(0,j,i),adapter.Zeta()(1,j,i),.5);
        for (int k=-sRho+1;k<=0;k++) {
            segmentU(0,k,j,i)=adapter.U()(0,k,j,i);
            segmentU(1,k,j,i)=(float)NumericalHelpers::interpolateTime(adapter.U()(0,k,j,i),adapter.U()(1,k,j,i),.5);
            segmentV(0,k,j,i)=adapter.V()(0,k,j,i);
            segmentV(1,k,j,i)=(float)NumericalHelpers::interpolateTime(adapter.V()(0,k,j,i),adapter.V()(1,k,j,i),.5);
        }
        for (int k=-sW+1;k<=0;k++) {
            segmentW(0,k,j,i)=adapter.W()(0,k,j,i);
            segmentW(1,k,j,i)=(float)NumericalHelpers::interpolateTime(adapter.W()(0,k,j,i),adapter.W()(1,k,j,i),.5);
            segmentAkt(0,k,j,i)=adapter.AKT()(0,k,j,i);
            segmentAkt(1,k,j,i)=(float)NumericalHelpers::interpolateTime(adapter.AKT()(0,k,j,i),adapter.AKT()(1,k,j,i),.5);
        }
    }

    config_data config{};
    config.random=false; config.randomSeed=5489; config.deltat=end-start; config.dti=30;
    config.survprob=0; config.tau0=86400; config.crid=1; config.sigma=.1;
    config.shoreLimit=.25; config.upperClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.lowerClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.horizontalClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.trackingDirection=Config::TRACKING_FORWARD;
    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();

    Particle continuousForward(100,-.5,.25,.25,start);
    continuousForward.move(&config,0,adapter.OceanTime(),adapter.Mask(),adapter.LonRad(),adapter.LatRad(),
                           adapter.SW(),adapter.DepthIntervals(),adapter.H(),adapter.Zeta(),adapter.U(),
                           adapter.V(),adapter.W(),adapter.AKT());
    Particle restartedForward(100,-.5,.25,.25,start);
    restartedForward.move(&config,0,segmentTime,adapter.Mask(),adapter.LonRad(),adapter.LatRad(),
                          adapter.SW(),adapter.DepthIntervals(),adapter.H(),segmentZeta,segmentU,
                          segmentV,segmentW,segmentAkt);
    config.restartCheckpoint=checkpoint;
    restartedForward.move(&config,0,adapter.OceanTime(),adapter.Mask(),adapter.LonRad(),adapter.LatRad(),
                          adapter.SW(),adapter.DepthIntervals(),adapter.H(),adapter.Zeta(),adapter.U(),
                          adapter.V(),adapter.W(),adapter.AKT());
    assert(std::abs(restartedForward.I()-continuousForward.I())<1e-9);
    assert(std::abs(restartedForward.J()-continuousForward.J())<1e-9);
    assert(std::abs(restartedForward.K()-continuousForward.K())<1e-9);
    assert(std::abs(restartedForward.Age()-continuousForward.Age())<1e-9);

    segmentTime(0)=checkpoint; segmentTime(1)=end;
    for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
        segmentZeta(0,j,i)=(float)NumericalHelpers::interpolateTime(adapter.Zeta()(0,j,i),adapter.Zeta()(1,j,i),.5);
        segmentZeta(1,j,i)=adapter.Zeta()(1,j,i);
        for (int k=-sRho+1;k<=0;k++) {
            segmentU(0,k,j,i)=(float)NumericalHelpers::interpolateTime(adapter.U()(0,k,j,i),adapter.U()(1,k,j,i),.5);
            segmentU(1,k,j,i)=adapter.U()(1,k,j,i);
            segmentV(0,k,j,i)=(float)NumericalHelpers::interpolateTime(adapter.V()(0,k,j,i),adapter.V()(1,k,j,i),.5);
            segmentV(1,k,j,i)=adapter.V()(1,k,j,i);
        }
        for (int k=-sW+1;k<=0;k++) {
            segmentW(0,k,j,i)=(float)NumericalHelpers::interpolateTime(adapter.W()(0,k,j,i),adapter.W()(1,k,j,i),.5);
            segmentW(1,k,j,i)=adapter.W()(1,k,j,i);
            segmentAkt(0,k,j,i)=(float)NumericalHelpers::interpolateTime(adapter.AKT()(0,k,j,i),adapter.AKT()(1,k,j,i),.5);
            segmentAkt(1,k,j,i)=adapter.AKT()(1,k,j,i);
        }
    }
    config.trackingDirection=Config::TRACKING_BACKWARD;
    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();
    Particle continuousBackward(101,-.5,.25,.25,1,0,end);
    continuousBackward.move(&config,1,adapter.OceanTime(),adapter.Mask(),adapter.LonRad(),adapter.LatRad(),
                            adapter.SW(),adapter.DepthIntervals(),adapter.H(),adapter.Zeta(),adapter.U(),
                            adapter.V(),adapter.W(),adapter.AKT());
    Particle restartedBackward(101,-.5,.25,.25,1,0,end);
    restartedBackward.move(&config,1,segmentTime,adapter.Mask(),adapter.LonRad(),adapter.LatRad(),
                           adapter.SW(),adapter.DepthIntervals(),adapter.H(),segmentZeta,segmentU,
                           segmentV,segmentW,segmentAkt);
    config.restartCheckpoint=checkpoint;
    restartedBackward.move(&config,1,adapter.OceanTime(),adapter.Mask(),adapter.LonRad(),adapter.LatRad(),
                           adapter.SW(),adapter.DepthIntervals(),adapter.H(),adapter.Zeta(),adapter.U(),
                           adapter.V(),adapter.W(),adapter.AKT());
    assert(std::abs(restartedBackward.I()-continuousBackward.I())<1e-9);
    assert(std::abs(restartedBackward.J()-continuousBackward.J())<1e-9);
    assert(std::abs(restartedBackward.K()-continuousBackward.K())<1e-9);
    assert(std::abs(restartedBackward.Age()-continuousBackward.Age())<1e-9);
}

#endif
