#include "../EnvironmentalRegridder.hpp"
#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace Array;

int main() {
    Array2<double> lon(2,2),lat(2,2);
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) { lon(j,i)=10+i; lat(j,i)=40+j; }
    Array3<float> affine(2,2,2),constant(2,2,2);
    for (int t=0;t<2;t++) for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        affine(t,j,i)=static_cast<float>(2*lon(j,i)+3*lat(j,i)+5*t);
        constant(t,j,i)=7.25f;
    }
    Array2<double> targetLon(3,3),targetLat(3,3);
    for (int j=0;j<3;j++) for (int i=0;i<3;i++) { targetLon(j,i)=10+.5*i; targetLat(j,i)=40+.5*j; }
    auto interpolated=EnvironmentalRegridder::bilinearGeographic(lon,lat,affine,targetLon,targetLat);
    auto preserved=EnvironmentalRegridder::bilinearGeographic(lon,lat,constant,targetLon,targetLat);
    for (int t=0;t<2;t++) for (int j=0;j<3;j++) for (int i=0;i<3;i++) {
        double expected=2*targetLon(j,i)+3*targetLat(j,i)+5*t;
        assert(std::abs(interpolated(t,j,i)-expected)<1.e-6);
        assert(preserved(t,j,i)==7.25f);
    }
    Array2<double> outsideLon(1,1),outsideLat(1,1); outsideLon(0,0)=12; outsideLat(0,0)=40.5;
    bool rejected=false;
    try { EnvironmentalRegridder::bilinearGeographic(lon,lat,affine,outsideLon,outsideLat); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    lon(1,0)=10.1;
    rejected=false;
    try { EnvironmentalRegridder::bilinearGeographic(lon,lat,affine,targetLon,targetLat); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);

    Array2<double> cyclicLon(2,2),cyclicLat(2,2),cyclicTargetLon(1,3),cyclicTargetLat(1,3);
    Array3<float> cyclicField(1,2,2);
    for (int j=0;j<2;j++) {
        cyclicLon(j,0)=179; cyclicLon(j,1)=-179; cyclicLat(j,0)=cyclicLat(j,1)=10+j;
        cyclicField(0,j,0)=0; cyclicField(0,j,1)=2;
    }
    cyclicTargetLon(0,0)=179; cyclicTargetLon(0,1)=180; cyclicTargetLon(0,2)=-179;
    cyclicTargetLat(0,0)=cyclicTargetLat(0,1)=cyclicTargetLat(0,2)=10.5;
    auto cyclic=EnvironmentalRegridder::bilinearGeographic(cyclicLon,cyclicLat,cyclicField,
                                                            cyclicTargetLon,cyclicTargetLat);
    assert(cyclic(0,0,0)==0 && cyclic(0,0,1)==1 && cyclic(0,0,2)==2);

    Array2<double> descendingLon(2,2),descendingLat(2,2),descendingTargetLon(1,1),descendingTargetLat(1,1);
    Array3<float> descendingField(1,2,2);
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        descendingLon(j,i)=11-i; descendingLat(j,i)=41-j;
        descendingField(0,j,i)=static_cast<float>(2*descendingLon(j,i)+3*descendingLat(j,i));
    }
    descendingTargetLon(0,0)=10.5; descendingTargetLat(0,0)=40.5;
    auto descending=EnvironmentalRegridder::bilinearGeographic(descendingLon,descendingLat,descendingField,
                                                                descendingTargetLon,descendingTargetLat);
    assert(std::abs(descending(0,0,0)-142.5)<1.e-6);

    Array2<double> curvedLon(2,2),curvedLat(2,2),curvedTargetLon(1,1),curvedTargetLat(1,1);
    curvedLon(0,0)=0; curvedLon(0,1)=2; curvedLon(1,0)=.2; curvedLon(1,1)=2.4;
    curvedLat(0,0)=0; curvedLat(0,1)=.1; curvedLat(1,0)=2; curvedLat(1,1)=2.2;
    curvedTargetLon(0,0)=.6875; curvedTargetLat(0,0)=1.54375;
    Array3<float> parameterField(1,2,2);
    parameterField(0,0,0)=1; parameterField(0,0,1)=3;
    parameterField(0,1,0)=4; parameterField(0,1,1)=6;
    auto curved=EnvironmentalRegridder::bilinearCurvilinearGeographic(curvedLon,curvedLat,parameterField,
                                                                      curvedTargetLon,curvedTargetLat);
    assert(std::abs(curved(0,0,0)-3.75)<1.e-6);

    curvedLon(0,0)=0; curvedLon(0,1)=1; curvedLon(1,0)=1; curvedLon(1,1)=0;
    curvedLat(0,0)=0; curvedLat(0,1)=0; curvedLat(1,0)=1; curvedLat(1,1)=1;
    curvedTargetLon(0,0)=.5; curvedTargetLat(0,0)=.5; rejected=false;
    try { EnvironmentalRegridder::bilinearCurvilinearGeographic(curvedLon,curvedLat,parameterField,
                                                                 curvedTargetLon,curvedTargetLat); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
}
