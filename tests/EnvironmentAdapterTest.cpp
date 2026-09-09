#include "../WeatherModelAdapterFactory.hpp"
#include "../WaveModelAdapterFactory.hpp"
#include <cassert>
#include <cstdio>

void createWRF(const std::string& name) {
    NcFile file(name,NcFile::replace,NcFile::nc4);
    NcDim time=file.addDim("Time",2),dateLength=file.addDim("DateStrLen",19);
    NcDim y=file.addDim("south_north",2),x=file.addDim("west_east",2);
    std::vector<NcDim> dims{time,y,x}; const char times[]="2026-01-01_00:00:002026-01-01_00:01:00";
    float lon[4]={10,11,10,11},lat[4]={40,40,41,41};
    float u[8]={1,2,3,4,5,6,7,8},v[8]={-1,-2,-3,-4,-5,-6,-7,-8};
    file.addVar("Times",ncChar,{time,dateLength}).putVar(times);
    NcVar longitude=file.addVar("XLONG",ncFloat,{y,x}); longitude.putAtt("units","degrees_east"); longitude.putVar(lon);
    NcVar latitude=file.addVar("XLAT",ncFloat,{y,x}); latitude.putAtt("units","degrees_north"); latitude.putVar(lat);
    NcVar windU=file.addVar("U10",ncFloat,dims); windU.putAtt("units","m s-1"); windU.putVar(u);
    NcVar windV=file.addVar("V10",ncFloat,dims); windV.putAtt("units","m s-1"); windV.putVar(v);
    float cosine[4]={0,0,0,0},sine[4]={1,1,1,1};
    file.addVar("COSALPHA",ncFloat,{y,x}).putVar(cosine); file.addVar("SINALPHA",ncFloat,{y,x}).putVar(sine);
}

void createWW3(const std::string& name,const std::string& velocityUnits="m s-1",
               const std::string& timeUnits="minutes since 1968-05-23 00:00:00 UTC") {
    NcFile file(name,NcFile::replace,NcFile::nc4);
    NcDim time=file.addDim("time",2),y=file.addDim("latitude",2),x=file.addDim("longitude",2);
    std::vector<NcDim> dims{time,y,x}; double times[2]={0,1},lon[2]={350,11},lat[2]={40,41};
    float u[8]={.1,.2,.3,.4,.5,.6,.7,.8},v[8]={-.1,-.2,-.3,-.4,-.5,-.6,-.7,-.8};
    NcVar timeVar=file.addVar("time",ncDouble,time); timeVar.putAtt("units",timeUnits); timeVar.putVar(times);
    NcVar longitude=file.addVar("longitude",ncDouble,x); longitude.putAtt("units","degrees_east"); longitude.putVar(lon);
    NcVar latitude=file.addVar("latitude",ncDouble,y); latitude.putAtt("units","degrees_north"); latitude.putVar(lat);
    NcVar stokesU=file.addVar("eastward_surface_stokes_drift",ncFloat,dims); stokesU.putAtt("units",velocityUnits); stokesU.putVar(u);
    NcVar stokesV=file.addVar("northward_surface_stokes_drift",ncFloat,dims); stokesV.putAtt("units",velocityUnits); stokesV.putVar(v);
}

int main() {
    std::string wrfFile="wrf-test.nc",ww3File="ww3-test.nc"; createWRF(wrfFile); createWW3(ww3File);
    auto weather=WeatherModelAdapterFactory::create("WRF",wrfFile); weather->process();
    assert(weather->Time().Nx()==2 && weather->Time()(1)-weather->Time()(0)==60);
    assert(weather->WindU10()(1,1,1)==8 && weather->WindV10()(0,0,0)==1);
    assert(weather->Lon()(0,1)==11 && weather->Lat()(1,0)==41);
    auto wave=WaveModelAdapterFactory::create("WW3",ww3File); wave->process();
    assert(wave->Time().Nx()==2 && wave->Time()(0)==0 && wave->Time()(1)==60);
    assert(std::abs(wave->StokesU()(1,1,1)-.8)<1.e-6);
    assert(wave->Lon()(0,0)==-10 && wave->Lon()(0,1)==11 && wave->Lat()(1,0)==41);
    bool rejected=false; try { WeatherModelAdapterFactory::create("unknown",wrfFile); }
    catch (const std::runtime_error&) { rejected=true; } assert(rejected);
    std::string invalidFile="ww3-invalid-units.nc"; createWW3(invalidFile,"knots"); rejected=false;
    try { auto invalid=WaveModelAdapterFactory::create("WW3",invalidFile); invalid->process(); }
    catch (const std::runtime_error&) { rejected=true; } assert(rejected);
    std::string invalidTimeFile="ww3-invalid-time.nc"; createWW3(invalidTimeFile,"m s-1","months since 1968-05-23"); rejected=false;
    try { auto invalid=WaveModelAdapterFactory::create("WW3",invalidTimeFile); invalid->process(); }
    catch (const std::runtime_error&) { rejected=true; } assert(rejected);
    std::remove(wrfFile.c_str()); std::remove(ww3File.c_str()); std::remove(invalidFile.c_str());
    std::remove(invalidTimeFile.c_str());
}
