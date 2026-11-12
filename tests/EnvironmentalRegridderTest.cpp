#include "../EnvironmentalRegridder.hpp"
#include <cassert>
#include <cmath>
#include <limits>
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

    const int indexedEta=21,indexedXi=31;
    Array2<double> indexedLon(indexedEta,indexedXi),indexedLat(indexedEta,indexedXi);
    Array2<double> indexedTargetLon(indexedEta-1,indexedXi-1),indexedTargetLat(indexedEta-1,indexedXi-1);
    Array3<float> indexedField(1,indexedEta,indexedXi);
    for (int j=0;j<indexedEta;j++) for (int i=0;i<indexedXi;i++) {
        indexedLon(j,i)=170+.08*i+.006*j*j/indexedEta;
        indexedLat(j,i)=30+.07*j+.004*i*i/indexedXi;
        indexedField(0,j,i)=static_cast<float>(2*indexedLon(j,i)+3*indexedLat(j,i));
    }
    for (int j=0;j+1<indexedEta;j++) for (int i=0;i+1<indexedXi;i++) {
        indexedTargetLon(j,i)=.63*.38*indexedLon(j,i)+.37*.38*indexedLon(j,i+1)+
                              .63*.62*indexedLon(j+1,i)+.37*.62*indexedLon(j+1,i+1);
        indexedTargetLat(j,i)=.63*.38*indexedLat(j,i)+.37*.38*indexedLat(j,i+1)+
                              .63*.62*indexedLat(j+1,i)+.37*.62*indexedLat(j+1,i+1);
    }
    auto indexed=EnvironmentalRegridder::bilinearCurvilinearGeographic(indexedLon,indexedLat,indexedField,
                                                                        indexedTargetLon,indexedTargetLat);
    for (int j=0;j+1<indexedEta;j++) for (int i=0;i+1<indexedXi;i++)
        assert(std::abs(indexed(0,j,i)-(2*indexedTargetLon(j,i)+3*indexedTargetLat(j,i)))<1.e-4);

    Array2<double> projectedX(2,2),projectedY(2,2),geographicLon(1,1),geographicLat(1,1);
    Array3<float> projectedField(1,2,2);
    double x1=111319.49079327357,y1=111325.1428663851;
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        projectedX(j,i)=i*x1; projectedY(j,i)=j*y1;
        projectedField(0,j,i)=static_cast<float>(projectedX(j,i)/1000+2*projectedY(j,i)/1000);
    }
    geographicLon(0,0)=.5; geographicLat(0,0)=.5;
#ifdef WACOMM_USE_PROJ
    auto projected=EnvironmentalRegridder::bilinearProjected(projectedX,projectedY,projectedField,
                                                               geographicLon,geographicLat,"EPSG:3857");
    double targetX=55659.74539663678,targetY=55660.45186542152;
    assert(std::abs(projected(0,0,0)-(targetX/1000+2*targetY/1000))<1.e-4);
    Array2<double> projectedCurvedX(2,2),projectedCurvedY(2,2);
    projectedCurvedX(0,0)=targetX-1000; projectedCurvedX(0,1)=targetX+900;
    projectedCurvedX(1,0)=targetX-900; projectedCurvedX(1,1)=targetX+1000;
    projectedCurvedY(0,0)=targetY-1000; projectedCurvedY(0,1)=targetY-900;
    projectedCurvedY(1,0)=targetY+900; projectedCurvedY(1,1)=targetY+1000;
    Array3<float> projectedCurvedField(1,2,2);
    projectedCurvedField(0,0,0)=1; projectedCurvedField(0,0,1)=3;
    projectedCurvedField(0,1,0)=4; projectedCurvedField(0,1,1)=6;
    auto projectedCurved=EnvironmentalRegridder::bilinearProjected(projectedCurvedX,projectedCurvedY,
            projectedCurvedField,geographicLon,geographicLat,"EPSG:3857");
    assert(std::abs(projectedCurved(0,0,0)-3.5)<1.e-5);
#endif
    Array2<double> sourceBoundsLon(3,3),sourceBoundsLat(3,3);
    for (int j=0;j<3;j++) for (int i=0;i<3;i++) {
        sourceBoundsLon(j,i)=i; sourceBoundsLat(j,i)=j;
    }
    Array3<float> sourceAverage(2,2,2);
    sourceAverage(0,0,0)=1; sourceAverage(0,0,1)=3;
    sourceAverage(0,1,0)=5; sourceAverage(0,1,1)=7;
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) sourceAverage(1,j,i)=-2*sourceAverage(0,j,i);
    Array2<double> targetBoundsLon(2,2),targetBoundsLat(2,2);
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        targetBoundsLon(j,i)=2*i; targetBoundsLat(j,i)=2*j;
    }
    auto conservative=EnvironmentalRegridder::conservativeRectilinearGeographicCellAverage(
            sourceBoundsLon,sourceBoundsLat,sourceAverage,targetBoundsLon,targetBoundsLat);
    double lowerArea=std::sin(3.14159265358979323846/180.0);
    double upperArea=std::sin(2*3.14159265358979323846/180.0)-lowerArea;
    double expected=(4*lowerArea+12*upperArea)/(2*(lowerArea+upperArea));
    assert(std::abs(conservative(0,0,0)-expected)<1.e-5);
    assert(std::abs(conservative(1,0,0)+2*expected)<1.e-5);
    Array3<float> constantAverage(2,2,2); constantAverage=4.25f;
    conservative=EnvironmentalRegridder::conservativeRectilinearGeographicCellAverage(
            sourceBoundsLon,sourceBoundsLat,constantAverage,targetBoundsLon,targetBoundsLat);
    assert(std::abs(conservative(0,0,0)-4.25)<1.e-6);
    for (int j=0;j<3;j++) { sourceBoundsLon(j,0)=179; sourceBoundsLon(j,1)=180; sourceBoundsLon(j,2)=-179; }
    targetBoundsLon(0,0)=179; targetBoundsLon(1,0)=179;
    targetBoundsLon(0,1)=-179; targetBoundsLon(1,1)=-179;
    conservative=EnvironmentalRegridder::conservativeRectilinearGeographicCellAverage(
            sourceBoundsLon,sourceBoundsLat,constantAverage,targetBoundsLon,targetBoundsLat);
    assert(std::abs(conservative(0,0,0)-4.25)<1.e-6);
    for (int j=0;j<3;j++) for (int i=0;i<3;i++) sourceBoundsLon(j,i)=i;
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) targetBoundsLon(j,i)=2*i;
    Array2<double> outsideBoundsLon(2,2),outsideBoundsLat(2,2);
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        outsideBoundsLon(j,i)=3*i; outsideBoundsLat(j,i)=2*j;
    }
    rejected=false;
    try { EnvironmentalRegridder::conservativeRectilinearGeographicCellAverage(
            sourceBoundsLon,sourceBoundsLat,sourceAverage,outsideBoundsLon,outsideBoundsLat); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    Array2<double> invalidBoundsLon=sourceBoundsLon;
    invalidBoundsLon(1,1)=.5; rejected=false;
    try { EnvironmentalRegridder::conservativeRectilinearGeographicCellAverage(
            invalidBoundsLon,sourceBoundsLat,sourceAverage,targetBoundsLon,targetBoundsLat); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    Array2<double> curvilinearBoundsLon(4,4),curvilinearBoundsLat(4,4),activeFraction(3,3);
    Array3<float> curvilinearAverage(1,3,3);
    const double radians=3.14159265358979323846/180;
    for (int j=0;j<4;j++) for (int i=0;i<4;i++) {
        curvilinearBoundsLon(j,i)=i; curvilinearBoundsLat(j,i)=j;
    }
    activeFraction=1.0;
    for (int j=0;j<3;j++) for (int i=0;i<3;i++)
        curvilinearAverage(0,j,i)=static_cast<float>((i+.5)*radians);
    Array2<double> refinedLon(3,3),refinedLat(3,3);
    for (int j=0;j<3;j++) for (int i=0;i<3;i++) {
        refinedLon(j,i)=1+.5*i; refinedLat(j,i)=1+.5*j;
    }
    auto firstOrder=EnvironmentalRegridder::conservativeCurvilinearGeographicCellAverage(
            curvilinearBoundsLon,curvilinearBoundsLat,curvilinearAverage,activeFraction,refinedLon,refinedLat);
    auto secondOrder=EnvironmentalRegridder::conservativeCurvilinearGeographicCellAverage(
            curvilinearBoundsLon,curvilinearBoundsLat,curvilinearAverage,activeFraction,refinedLon,refinedLat,true);
    assert(std::abs(firstOrder(0,0,0)-1.5*radians)<1.e-7);
    assert(std::abs(secondOrder(0,0,0)-1.25*radians)<1.e-7);
    assert(std::abs(secondOrder(0,0,1)-1.75*radians)<1.e-7);
    double firstIntegral=0,secondIntegral=0;
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        double area=.5*radians*(std::sin((1+.5*(j+1))*radians)-std::sin((1+.5*j)*radians));
        firstIntegral+=area*firstOrder(0,j,i); secondIntegral+=area*secondOrder(0,j,i);
    }
    assert(std::abs(firstIntegral-secondIntegral)<1.e-12);
    curvilinearAverage=4.0f; activeFraction=1.0;
    Array2<double> coarseLon(2,2),coarseLat(2,2);
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) { coarseLon(j,i)=3*i; coarseLat(j,i)=3*j; }
    for (int j=1;j<3;j++) for (int i=1;i<3;i++) {
        curvilinearBoundsLon(j,i)+=.08*j; curvilinearBoundsLat(j,i)+=.04*i;
    }
    auto warpedConstant=EnvironmentalRegridder::conservativeCurvilinearGeographicCellAverage(
            curvilinearBoundsLon,curvilinearBoundsLat,curvilinearAverage,activeFraction,coarseLon,coarseLat);
    assert(std::abs(warpedConstant(0,0,0)-4)<1.e-6);
    for (int j=0;j<4;j++) for (int i=0;i<4;i++) {
        curvilinearBoundsLon(j,i)=i; curvilinearBoundsLat(j,i)=j;
    }
    activeFraction(0,0)=.5;
    auto masked=EnvironmentalRegridder::conservativeCurvilinearGeographicCellAverage(
            curvilinearBoundsLon,curvilinearBoundsLat,curvilinearAverage,activeFraction,coarseLon,coarseLat);
    double fullMeasure=3*radians*std::sin(3*radians),maskedMeasure=.5*radians*std::sin(radians);
    assert(std::abs(masked(0,0,0)-4*(1-maskedMeasure/fullMeasure))<1.e-6);
    activeFraction=1.0; activeFraction(0,0)=0; curvilinearAverage(0,0,0)=std::numeric_limits<float>::quiet_NaN();
    masked=EnvironmentalRegridder::conservativeCurvilinearGeographicCellAverage(
            curvilinearBoundsLon,curvilinearBoundsLat,curvilinearAverage,activeFraction,coarseLon,coarseLat);
    assert(std::isfinite(masked(0,0,0)) && std::abs(masked(0,0,0)-4*(1-2*maskedMeasure/fullMeasure))<1.e-6);
    curvilinearAverage=4.0f; activeFraction=1.0;
    Array2<double> conservativeCyclicLon(2,3),conservativeCyclicLat(2,3),cyclicFraction(1,2);
    Array2<double> conservativeCyclicTargetLon(2,2),conservativeCyclicTargetLat(2,2);
    Array3<float> cyclicAverage(1,1,2); cyclicAverage=2.5f; cyclicFraction=1.0;
    for (int j=0;j<2;j++) {
        conservativeCyclicLon(j,0)=179; conservativeCyclicLon(j,1)=180; conservativeCyclicLon(j,2)=-179;
        conservativeCyclicLat(j,0)=j; conservativeCyclicLat(j,1)=j; conservativeCyclicLat(j,2)=j;
        conservativeCyclicTargetLon(j,0)=179; conservativeCyclicTargetLon(j,1)=-179;
        conservativeCyclicTargetLat(j,0)=j; conservativeCyclicTargetLat(j,1)=j;
    }
    auto cyclicConservative=EnvironmentalRegridder::conservativeCurvilinearGeographicCellAverage(
            conservativeCyclicLon,conservativeCyclicLat,cyclicAverage,cyclicFraction,
            conservativeCyclicTargetLon,conservativeCyclicTargetLat);
    assert(std::abs(cyclicConservative(0,0,0)-2.5)<1.e-6);
    Array2<double> foldedLon=coarseLon;
    foldedLon(1,0)=3; foldedLon(1,1)=0; rejected=false;
    try { EnvironmentalRegridder::conservativeCurvilinearGeographicCellAverage(
            curvilinearBoundsLon,curvilinearBoundsLat,curvilinearAverage,activeFraction,foldedLon,coarseLat); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
#ifdef WACOMM_USE_PROJ
    rejected=false;
    try { EnvironmentalRegridder::bilinearProjected(projectedX,projectedY,projectedField,
                                                      geographicLon,geographicLat,"EPSG:invalid"); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
#else
    rejected=false;
    try { EnvironmentalRegridder::bilinearProjected(projectedX,projectedY,projectedField,
                                                      geographicLon,geographicLat,"EPSG:3857"); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
#endif

    curvedLon(0,0)=0; curvedLon(0,1)=1; curvedLon(1,0)=1; curvedLon(1,1)=0;
    curvedLat(0,0)=0; curvedLat(0,1)=0; curvedLat(1,0)=1; curvedLat(1,1)=1;
    curvedTargetLon(0,0)=.5; curvedTargetLat(0,0)=.5; rejected=false;
    try { EnvironmentalRegridder::bilinearCurvilinearGeographic(curvedLon,curvedLat,parameterField,
                                                                 curvedTargetLon,curvedTargetLat); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
}
