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
}
