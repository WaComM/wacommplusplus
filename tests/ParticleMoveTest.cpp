#include "../Particle.hpp"
#include "../Config.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

int main() {
    Array1<double> oceanTime(2); oceanTime(0)=0; oceanTime(1)=65;
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
    for (int k=-1;k<=0;k++) for (int j=0;j<2;j++) for (int i=0;i<2;i++) u(1,k,j,i)=2.0f;
    config_data config{};
    config.random=false; config.deltat=3600; config.dti=30; config.survprob=0;
    config.tau0=86400; config.crid=1; config.sigma=3.46; config.shoreLimit=.25;
    config.upperClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.lowerClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.horizontalClosure=Config::CLOSURE_MODE_CONSTRAINT;
    config.trackingDirection=Config::TRACKING_FORWARD;
    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();

    Particle full(1,-.5,.25,.25,0);
    full.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
    assert(std::abs(full.Age()-65)<1e-12);
    double distanceTerm=std::pow(std::sin(0.5*0.017453292519943295),2)*
                        std::cos(0.017453292519943295);
    double xdist=2.0*std::atan2(std::sqrt(distanceTerm),std::sqrt(1.0-distanceTerm))*6371000.0;
    assert(std::abs(full.I()-(.25+65.0/xdist))<1e-10);

    Particle emitted(2,-.5,.25,.25,40);
    emitted.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
    assert(std::abs(emitted.Age()-25)<1e-12);
    double emittedDistance=(65.0*65.0-40.0*40.0)/65.0;
    assert(std::abs(emitted.I()-(.25+emittedDistance/xdist))<1e-10);

    config.trackingDirection=Config::TRACKING_BACKWARD;
    Particle backward(3,-.5,full.J(),full.I(),65);
    backward.move(&config,1,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
    assert(std::abs(backward.Age()-65)<1e-12);
    assert(std::abs(backward.I()-.25)<1e-10);

    config.trackingDirection=Config::TRACKING_FORWARD;
    config.random=true;
    config.randomSeed=5489;
    Particle stochastic0(4,-.5,.25,.25,0),stochastic1(4,-.5,.25,.25,0);
    stochastic0.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
    stochastic1.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
    assert(stochastic0.I()==stochastic1.I() && stochastic0.J()==stochastic1.J() &&
           stochastic0.K()==stochastic1.K());
    config.randomSeed=5490;
    Particle stochastic2(4,-.5,.25,.25,0);
    stochastic2.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
    assert(stochastic0.I()!=stochastic2.I() || stochastic0.J()!=stochastic2.J() ||
           stochastic0.K()!=stochastic2.K());

    Array4<float> zeroU(2,2,2,2,0,-1,0,0); zeroU=0.0f;
    config.randomSeed=5489;
    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();
    Particle stochasticFull(8,-.5,.25,.25,0);
    stochasticFull.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,zeroU,v,w,akt);
    Array1<double> stochasticEarlyTime(2); stochasticEarlyTime(0)=0; stochasticEarlyTime(1)=30;
    Particle stochasticRestarted(8,-.5,.25,.25,0);
    stochasticRestarted.move(&config,0,stochasticEarlyTime,mask,lonRad,latRad,sW,depthIntervals,h,
                             zeta,zeroU,v,w,akt);
    config.restartCheckpoint=30;
    stochasticRestarted.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,
                             zeta,zeroU,v,w,akt);
    assert(stochasticRestarted.I()==stochasticFull.I());
    assert(stochasticRestarted.J()==stochasticFull.J());
    assert(stochasticRestarted.K()==stochasticFull.K());
    assert(stochasticRestarted.Age()==stochasticFull.Age());

    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();
    std::vector<Particle> serialParticles,parallelParticles;
    for (int idx=0;idx<64;idx++) {
        serialParticles.push_back(Particle(100+idx,-.5,.25,.25,0));
        parallelParticles.push_back(Particle(100+idx,-.5,.25,.25,0));
        serialParticles[idx].move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,
                                  zeta,zeroU,v,w,akt);
    }
    #pragma omp parallel for default(none) shared(parallelParticles, config, oceanTime, mask, lonRad, latRad, sW, depthIntervals, h, zeta, zeroU, v, w, akt)
    for (int idx=0;idx<64;idx++) {
        parallelParticles[idx].move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,
                                    zeta,zeroU,v,w,akt);
    }
    for (int idx=0;idx<64;idx++) {
        assert(parallelParticles[idx].I()==serialParticles[idx].I());
        assert(parallelParticles[idx].J()==serialParticles[idx].J());
        assert(parallelParticles[idx].K()==serialParticles[idx].K());
    }

    config.random=false;
    config.randomSeed=5489;
    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();
    Array1<double> earlyTime(2); earlyTime(0)=0; earlyTime(1)=40;
    Array4<float> earlyU(2,2,2,2,0,-1,0,0); earlyU=0.0f;
    for (int k=-1;k<=0;k++) for (int j=0;j<2;j++) for (int i=0;i<2;i++)
        earlyU(1,k,j,i)=80.0f/65.0f;
    Particle restartedForward(5,-.5,.25,.25,0);
    restartedForward.move(&config,0,earlyTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,earlyU,v,w,akt);
    config.restartCheckpoint=40;
    restartedForward.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
    assert(std::abs(restartedForward.I()-full.I())<1e-8);
    assert(std::abs(restartedForward.Age()-full.Age())<1e-12);

    config.trackingDirection=Config::TRACKING_BACKWARD;
    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();
    Array1<double> lateTime(2); lateTime(0)=40; lateTime(1)=65;
    Array4<float> lateU(2,2,2,2,0,-1,0,0); lateU=0.0f;
    for (int k=-1;k<=0;k++) for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        lateU(0,k,j,i)=80.0f/65.0f;
        lateU(1,k,j,i)=2.0f;
    }
    Particle restartedBackward(6,-.5,full.J(),full.I(),65);
    restartedBackward.move(&config,1,lateTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,lateU,v,w,akt);
    config.restartCheckpoint=40;
    restartedBackward.move(&config,1,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,w,akt);
    assert(std::abs(restartedBackward.I()-.25)<1e-8);
    assert(std::abs(restartedBackward.Age()-65)<1e-12);

    config.trackingDirection=Config::TRACKING_FORWARD;
    config.restartCheckpoint=std::numeric_limits<double>::quiet_NaN();
    Array1<double> verticalTime(2); verticalTime(0)=0; verticalTime(1)=1;
    Array4<float> verticalW(2,3,2,2,0,-2,0,0); verticalW=0.0f;
    for (int t=0;t<2;t++) for (int j=0;j<2;j++) for (int i=0;i<2;i++)
        verticalW(t,0,j,i)=2.0f;
    Particle vertical(7,-.5,.25,.25,0);
    vertical.move(&config,0,verticalTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,u,v,verticalW,akt);
    assert(std::abs(vertical.K()+.48)<1e-12);

    config.sv=0;
    config.driftModel=1;
    config.driftObjectType=static_cast<std::uint16_t>(DriftObjectType::PERSON_IN_WATER);
    config.driftSide=static_cast<std::int8_t>(DriftSide::RIGHT);
    config.hasWind=true; config.windU10=10; config.windV10=0;
    Particle leewayForward(9,-.5,.25,.25,0);
    leewayForward.Drift(DriftObjectType::PERSON_IN_WATER,DriftSide::RIGHT);
    leewayForward.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,zeroU,v,w,akt);
    assert(leewayForward.I()>.25 && leewayForward.J()>.25);
    config.trackingDirection=Config::TRACKING_BACKWARD;
    Particle leewayBackward(10,-.5,leewayForward.J(),leewayForward.I(),65);
    leewayBackward.Drift(DriftObjectType::PERSON_IN_WATER,DriftSide::RIGHT);
    leewayBackward.move(&config,1,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,zeroU,v,w,akt);
    assert(std::abs(leewayBackward.I()-.25)<1.e-8);
    assert(std::abs(leewayBackward.J()-.25)<1.e-8);

    config.leewayCoefficientEnsemble=true;
    config.trackingDirection=Config::TRACKING_FORWARD;
    Particle ensemble0(20,-.5,.25,.25,0),ensemble0Repeat(20,-.5,.25,.25,0);
    Particle ensemble1(21,-.5,.25,.25,0);
    ensemble0.Drift(DriftObjectType::PERSON_IN_WATER,DriftSide::RIGHT);
    ensemble0Repeat.Drift(DriftObjectType::PERSON_IN_WATER,DriftSide::RIGHT);
    ensemble1.Drift(DriftObjectType::PERSON_IN_WATER,DriftSide::RIGHT);
    ensemble0.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,zeroU,v,w,akt);
    ensemble0Repeat.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,zeroU,v,w,akt);
    ensemble1.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,zeroU,v,w,akt);
    assert(ensemble0.I()==ensemble0Repeat.I() && ensemble0.J()==ensemble0Repeat.J());
    assert(ensemble0.I()!=ensemble1.I() || ensemble0.J()!=ensemble1.J());
    config.trackingDirection=Config::TRACKING_BACKWARD;
    Particle ensembleBackward(20,-.5,ensemble0.J(),ensemble0.I(),65);
    ensembleBackward.Drift(DriftObjectType::PERSON_IN_WATER,DriftSide::RIGHT);
    ensembleBackward.move(&config,1,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,zeroU,v,w,akt);
    assert(std::abs(ensembleBackward.I()-.25)<1.e-8);
    assert(std::abs(ensembleBackward.J()-.25)<1.e-8);
    config.leewayCoefficientEnsemble=false;

    Array3<float> windU(2,2,2),windV(2,2,2),stokesU(2,2,2),stokesV(2,2,2);
    windU=10.0f; windV=0.0f; stokesU=.2f; stokesV=-.1f;
    config.trackingDirection=Config::TRACKING_FORWARD;
    Particle coupledForward(11,-.5,.25,.25,0);
    coupledForward.Drift(DriftObjectType::PERSON_IN_WATER,DriftSide::RIGHT);
    coupledForward.move(&config,0,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,zeroU,v,w,akt,
                        &windU,&windV,&stokesU,&stokesV);
    assert(coupledForward.I()>leewayForward.I());
    config.trackingDirection=Config::TRACKING_BACKWARD;
    Particle coupledBackward(12,-.5,coupledForward.J(),coupledForward.I(),65);
    coupledBackward.Drift(DriftObjectType::PERSON_IN_WATER,DriftSide::RIGHT);
    coupledBackward.move(&config,1,oceanTime,mask,lonRad,latRad,sW,depthIntervals,h,zeta,zeroU,v,w,akt,
                         &windU,&windV,&stokesU,&stokesV);
    assert(std::abs(coupledBackward.I()-.25)<1.e-8);
    assert(std::abs(coupledBackward.J()-.25)<1.e-8);
}
