#include "../OceanModelAdapters/HYCOMAdapter.hpp"
#include "../OceanModelAdapters/NEMOAdapter.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace netCDF;

void createFixture(const string &fileName, bool nemo) {
    NcFile file(fileName,NcFile::replace,NcFile::nc4);
    NcDim timeDim=file.addDim("time",2),depthDim=file.addDim("depth",2);
    NcDim latDim=file.addDim("lat",2),lonDim=file.addDim("lon",3);
    NcVar time=file.addVar(nemo ? "time_counter" : "MT",ncDouble,timeDim);
    NcVar depth=file.addVar(nemo ? "deptht" : "depth",ncDouble,depthDim);
    NcVar lat=file.addVar(nemo ? "nav_lat" : "lat",ncDouble,latDim);
    NcVar lon=file.addVar(nemo ? "nav_lon" : "lon",ncDouble,lonDim);
    vector<NcDim> velocityDims{timeDim,depthDim,latDim,lonDim};
    NcVar u=file.addVar(nemo ? "uo" : "water_u",ncFloat,velocityDims);
    NcVar v=file.addVar(nemo ? "vo" : "water_v",ncFloat,velocityDims);
    vector<NcDim> surfaceDims{timeDim,latDim,lonDim};
    NcVar zeta=file.addVar(nemo ? "zos" : "surf_el",ncFloat,surfaceDims);
    NcVar mask;
    if (nemo) mask=file.addVar("tmask",ncFloat,velocityDims);
    double times[2]={0,3600},depths[2]={5,25},latitudes[2]={40,41};
    double longitudes[3]={nemo ? 10.0 : 350.0,11,12};
    vector<float> values(24,.25f),zeros(12,0);
    time.putVar(times); depth.putVar(depths); lat.putVar(latitudes); lon.putVar(longitudes);
    u.putVar(values.data()); std::fill(values.begin(),values.end(),-.1f); v.putVar(values.data());
    zeta.putVar(zeros.data());
    if (nemo) { std::fill(values.begin(),values.end(),1); values[0]=0; mask.putVar(values.data()); }
}

int main() {
    string nemoFile="structured-nemo-test.nc",hycomFile="structured-hycom-test.nc";
    createFixture(nemoFile,true); createFixture(hycomFile,false);
    NEMOAdapter nemo(nemoFile); nemo.process();
    HYCOMAdapter hycom(hycomFile); hycom.process();
    assert(nemo.OceanTime().Nx()==2 && nemo.U().Ny()==2);
    assert(std::abs(nemo.U()(0,-1,0,0)-.25)<1e-6);
    assert(std::abs(nemo.V()(0,-1,0,0)+.1)<1e-6);
    assert(nemo.Mask()(0,0)==0 && nemo.Mask()(0,1)==1);
    assert(hycom.Lon()(0,0)==-10);
    assert(hycom.W()(0,-2,0,0)==0 && hycom.AKT()(0,-2,0,0)==0);
    std::remove(nemoFile.c_str()); std::remove(hycomFile.c_str());
}
