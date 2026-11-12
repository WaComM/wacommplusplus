// Regression test for the mapping between physical position (depth, lat, lon)
// and fractional grid coordinates (k, j, i) (docs/model.md#vertical-coordinate-mapping
// and docs/model.md#horizontal-position-mapping). deplatlon2kji must be the inverse
// of kji2deplatlon on the valid particle levels [-s_w+2, 0], on surface, interior,
// W-level, and bottom depths, and on rectilinear and curvilinear interpolation cells.

#include "../OceanModelAdapter.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

class FixtureAdapter: public OceanModelAdapter {
public:
    void process() override {}
};

// Declared round-trip tolerances
static const double depthTolerance=1e-9;   // m
static const double levelTolerance=1e-12;  // dimensionless k
static const double indexTolerance=1e-9;   // dimensionless j, i

static bool rejects(OceanModelAdapter& adapter, double dep, double lat, double lon, bool outside) {
    double k,j,i;
    try { adapter.deplatlon2kji(dep,lat,lon,k,j,i); }
    catch (const std::out_of_range&) { return outside; }
    catch (const std::invalid_argument&) { return !outside; }
    return false;
}

// Rotated, sheared, non-parallelogram curvilinear grid
static void curvilinearChecks(int nW, const double* sW) {
    const int nJ=5, nI=6;
    const double theta=0.5; // rad
    FixtureAdapter adapter;
    adapter.SW().Allocate(nW,-nW+1);
    for (int k=0;k<nW;k++) adapter.SW()(k-nW+1)=sW[k];
    adapter.W().Allocate(1,nW,nJ,nI,0,-nW+1,0,0); adapter.W()=0.0f;
    adapter.Mask().Allocate(nJ,nI); adapter.Mask()=1.0;
    adapter.Lon().Allocate(nJ,nI); adapter.Lat().Allocate(nJ,nI); adapter.H().Allocate(nJ,nI);
    adapter.LonRad().Allocate(nJ,nI); adapter.LatRad().Allocate(nJ,nI);
    for (int j=0;j<nJ;j++) for (int i=0;i<nI;i++) {
        adapter.Lon()(j,i)=14+0.05*(i*std::cos(theta)-j*std::sin(theta))+0.004*i*j;
        adapter.Lat()(j,i)=40.5+0.05*(i*std::sin(theta)+j*std::cos(theta))-0.003*i*i;
        adapter.LonRad()(j,i)=0.0174533*adapter.Lon()(j,i);
        adapter.LatRad()(j,i)=0.0174533*adapter.Lat()(j,i);
        adapter.H()(j,i)=50+7*i+3*j;
    }

    // (j, i) -> (lat, lon) -> (j, i) over the interpolation domain, at a fixed depth
    double k,j,i,dep,lat,lon;
    for (int n=0;n<=40;n++) for (int m=0;m<=50;m++) {
        double jTarget=(nJ-1)*n/40.0*(1-1e-6), iTarget=(nI-1)*m/50.0*(1-1e-6);
        adapter.kji2deplatlon(-0.37,jTarget,iTarget,dep,lat,lon);
        assert(dep!=1e37);
        adapter.deplatlon2kji(dep,lat,lon,k,j,i);
        assert(std::abs(j-jTarget)<=indexTolerance && std::abs(i-iTarget)<=indexTolerance);
        assert(std::abs(k+0.37)<=1e-9);
    }

    // Interior nodes map to integer indices
    adapter.deplatlon2kji(0,adapter.Lat()(2,3),adapter.Lon()(2,3),k,j,i);
    assert(std::abs(j-2)<=indexTolerance && std::abs(i-3)<=indexTolerance);

    // South and west node lines are inside, north and east node lines and beyond are outside
    adapter.deplatlon2kji(0,adapter.Lat()(0,2),adapter.Lon()(0,2),k,j,i);
    assert(std::abs(j)<=indexTolerance && std::abs(i-2)<=indexTolerance);
    adapter.deplatlon2kji(0,adapter.Lat()(2,0),adapter.Lon()(2,0),k,j,i);
    assert(std::abs(j-2)<=indexTolerance && std::abs(i)<=indexTolerance);
    assert(rejects(adapter,0,adapter.Lat()(nJ-1,2),adapter.Lon()(nJ-1,2),true));
    assert(rejects(adapter,0,adapter.Lat()(2,nI-1),adapter.Lon()(2,nI-1),true));
    assert(rejects(adapter,0,adapter.Lat()(nJ-1,nI-1),adapter.Lon()(nJ-1,nI-1),true));
    assert(rejects(adapter,0,adapter.Lat()(0,0)-0.01,adapter.Lon()(0,0),true));
    assert(rejects(adapter,0,45.0,20.0,true));
    assert(rejects(adapter,0,NAN,14.1,false));
    assert(rejects(adapter,0,40.6,INFINITY,false));
}

int main() {
    // Five W levels with non-uniform, surface-refined stretching
    const int nW=5;
    const double sW[nW]={-1.0,-0.6,-0.3,-0.1,0.0};

    FixtureAdapter adapter;
    adapter.SW().Allocate(nW,-nW+1);
    for (int k=0;k<nW;k++) adapter.SW()(k-nW+1)=sW[k];
    adapter.W().Allocate(1,nW,3,3,0,-nW+1,0,0); adapter.W()=0.0f;
    adapter.Mask().Allocate(3,3); adapter.Mask()=1.0;
    adapter.Lon().Allocate(3,3); adapter.Lat().Allocate(3,3); adapter.H().Allocate(3,3);
    adapter.LonRad().Allocate(3,3); adapter.LatRad().Allocate(3,3);
    for (int j=0;j<3;j++) for (int i=0;i<3;i++) {
        adapter.Lon()(j,i)=10+0.1*i; adapter.Lat()(j,i)=40+0.1*j;
        adapter.LonRad()(j,i)=0.0174533*adapter.Lon()(j,i);
        adapter.LatRad()(j,i)=0.0174533*adapter.Lat()(j,i);
        // Sloping bathymetry, so the bilinear h enters both directions
        adapter.H()(j,i)=80+20*i+10*j;
    }

    const double lat=40.13, lon=10.07;
    const int kBottom=-nW+2;
    double k,j,i,dep,outLat,outLon;

    // On a rectilinear grid the horizontal inverse is linear in each index
    adapter.deplatlon2kji(0,lat,lon,k,j,i);
    assert(std::abs(j-(lat-40)/0.1)<=indexTolerance && std::abs(i-(lon-10)/0.1)<=indexTolerance);

    // Surface
    adapter.deplatlon2kji(0,lat,lon,k,j,i);
    assert(k==0);
    adapter.kji2deplatlon(k,j,i,dep,outLat,outLon);
    assert(std::abs(dep)<=depthTolerance);

    // Depth of the lowest valid particle level at the mapped horizontal position
    adapter.kji2deplatlon(kBottom,j,i,dep,outLat,outLon);
    const double bottomDepth=-dep;
    assert(bottomDepth>0);

    // Depth -> k -> depth on surface-adjacent, interior, W-level, and bottom depths;
    // the sign of the input depth is not significant (positive down)
    int checked=0; double previousK=1;
    for (int n=1;n<=200;n++) {
        double target=bottomDepth*n/200.0;
        for (double signedTarget: {target,-target}) {
            adapter.deplatlon2kji(signedTarget,lat,lon,k,j,i);
            assert(k>=kBottom-levelTolerance && k<=0);
            adapter.kji2deplatlon(k,j,i,dep,outLat,outLon);
            assert(std::abs(-dep-target)<=depthTolerance);
            checked++;
        }
        // k decreases monotonically with depth
        assert(k<previousK);
        previousK=k;
    }
    assert(checked==400);

    // A depth exactly on the lowest valid particle level maps to it
    adapter.deplatlon2kji(bottomDepth,lat,lon,k,j,i);
    assert(std::abs(k-kBottom)<=levelTolerance);

    // Depths below the lowest valid particle level, or below h, clamp to it
    for (double below: {bottomDepth+1e-6,bottomDepth+5.0,1e4}) {
        adapter.deplatlon2kji(below,lat,lon,k,j,i);
        assert(k==kBottom);
    }

    // k -> depth -> k on the valid particle levels
    for (int n=0;n<=300;n++) {
        double kTarget=kBottom*n/300.0;
        adapter.kji2deplatlon(kTarget,j,i,dep,outLat,outLon);
        double kBack;
        adapter.deplatlon2kji(dep,lat,lon,kBack,j,i);
        assert(std::abs(kBack-kTarget)<=levelTolerance);
    }

    // Exact W levels map to integer k
    for (int level=kBottom;level<=0;level++) {
        adapter.kji2deplatlon(level,j,i,dep,outLat,outLon);
        adapter.deplatlon2kji(dep,lat,lon,k,j,i);
        assert(std::abs(k-level)<=levelTolerance);
    }

    // Non-finite depths are rejected
    bool rejected=false;
    try { adapter.deplatlon2kji(NAN,lat,lon,k,j,i); }
    catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);

    curvilinearChecks(nW,sW);
    return 0;
}
