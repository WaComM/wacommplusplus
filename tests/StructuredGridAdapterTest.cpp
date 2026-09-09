#include "../OceanModelAdapters/HYCOMAdapter.hpp"
#include "../OceanModelAdapters/NEMOAdapter.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>
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
    vector<float> values(24,.1f),zeros(12,0);
    time.putVar(times); depth.putVar(depths); lat.putVar(latitudes); lon.putVar(longitudes);
    for (size_t t=0;t<2;t++) for (size_t index=6;index<12;index++) values[t*12+index]=.2f;
    u.putVar(values.data());
    for (float &value:values) value=-value;
    v.putVar(values.data());
    zeta.putVar(zeros.data());
    if (nemo) { std::fill(values.begin(),values.end(),1); values[0]=0; mask.putVar(values.data()); }
}

int main() {
    string nemoFile="structured-nemo-test.nc",hycomFile="structured-hycom-test.nc";
    string nextFile="structured-next-test.nc",nextHycomFile="structured-next-hycom-test.nc";
    string olderFile="structured-older-test.nc";
    string invalidFile="structured-invalid-time-test.nc";
    createFixture(nemoFile,true); createFixture(hycomFile,false); createFixture(nextFile,true);
    createFixture(nextHycomFile,false);
    createFixture(olderFile,true); createFixture(invalidFile,false);
    {
        NcFile file(nextFile,NcFile::write);
        double times[2]={5400,7200}; file.getVar("time_counter").putVar(times);
        vector<float> values(24,.3f); file.getVar("uo").putVar(values.data());
    }
    {
        NcFile file(nextHycomFile,NcFile::write);
        double times[2]={5400,7200}; file.getVar("MT").putVar(times);
    }
    {
        NcFile file(olderFile,NcFile::write);
        double times[2]={-7200,-3600}; file.getVar("time_counter").putVar(times);
    }
    NEMOAdapter nemo(nemoFile); nemo.process();
    NEMOAdapter next(nextFile); next.process();
    NEMOAdapter older(olderFile); older.process();
    HYCOMAdapter hycom(hycomFile); hycom.process();
    HYCOMAdapter nextHycom(nextHycomFile); nextHycom.process();
    assert(nemo.OceanTime().Nx()==2 && nemo.U().Ny()==2);
    assert(nemo.SRho()(-1)==-1 && nemo.SRho()(0)==-.2);
    assert(std::abs(nemo.U()(0,-1,0,0)-.2)<1e-6);
    assert(std::abs(nemo.U()(0,0,0,0)-.1)<1e-6);
    assert(std::abs(nemo.V()(0,-1,0,0)+.2)<1e-6);
    assert(nemo.Mask()(0,0)==0 && nemo.Mask()(0,1)==1);
    nemo.appendBoundaryRecord(next,0,false);
    assert(nemo.OceanTime().Nx()==3 && nemo.OceanTime()(2)==5400);
    assert(std::abs(nemo.U()(2,-1,0,0)-.3)<1e-6);
    NEMOAdapter backward(nemoFile); backward.process();
    backward.appendBoundaryRecord(older,1,true);
    assert(backward.OceanTime().Nx()==3 && backward.OceanTime()(0)==-3600 && backward.OceanTime()(1)==0);
    bool rejected=false;
    try { backward.appendBoundaryRecord(older,2,true); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    assert(hycom.Lon()(0,0)==-10);
    assert(hycom.W()(0,-2,0,0)==0 && hycom.AKT()(0,-2,0,0)==0);
    hycom.appendBoundaryRecord(nextHycom,0,false);
    assert(hycom.OceanTime().Nx()==3 && hycom.OceanTime()(2)==5400);
    {
        NcFile file(invalidFile,NcFile::write);
        double descending[2]={3600,0}; file.getVar("MT").putVar(descending);
    }
    rejected=false;
    try { HYCOMAdapter invalid(invalidFile); invalid.process(); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    std::remove(nemoFile.c_str()); std::remove(hycomFile.c_str()); std::remove(nextFile.c_str());
    std::remove(nextHycomFile.c_str());
    std::remove(olderFile.c_str());
    std::remove(invalidFile.c_str());
}
