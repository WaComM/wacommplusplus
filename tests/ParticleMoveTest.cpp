#include "../Particle.hpp"
#include "../Config.hpp"

#include <cassert>
#include <cmath>
#include <limits>

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
}
