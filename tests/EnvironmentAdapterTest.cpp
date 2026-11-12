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

void createProjectedWW3(const std::string& name) {
    NcFile file(name,NcFile::replace,NcFile::nc4);
    NcDim time=file.addDim("time",1),y=file.addDim("y",2),x=file.addDim("x",2);
    double times[1]={0},xs[4]={0,111219.49079327357,100,111319.49079327357};
    double ys[4]={0,100,111225.1428663851,111325.1428663851};
    float u[4]={0,1,2,3},v[4]={0,-1,-2,-3};
    NcVar timeVar=file.addVar("time",ncDouble,time); timeVar.putAtt("units","seconds since 1968-05-23 00:00:00 UTC"); timeVar.putVar(times);
    NcVar xVar=file.addVar("x",ncDouble,{y,x}); xVar.putAtt("units","m"); xVar.putVar(xs);
    NcVar yVar=file.addVar("y",ncDouble,{y,x}); yVar.putAtt("units","m"); yVar.putVar(ys);
    NcVar stokesU=file.addVar("eastward_surface_stokes_drift",ncFloat,{time,y,x}); stokesU.putAtt("units","m s-1"); stokesU.putVar(u);
    NcVar stokesV=file.addVar("northward_surface_stokes_drift",ncFloat,{time,y,x}); stokesV.putAtt("units","m s-1"); stokesV.putVar(v);
}

void createProjectedWRF(const std::string& name) {
    NcFile file(name,NcFile::replace,NcFile::nc4);
    NcDim time=file.addDim("Time",1),dateLength=file.addDim("DateStrLen",19);
    NcDim y=file.addDim("south_north",2),x=file.addDim("west_east",2);
    const char times[]="2026-01-01_00:00:00";
    double xs[2]={0,111319.49079327357},ys[2]={0,111325.1428663851};
    float u[4]={0,1,2,3},v[4]={0,-1,-2,-3},cosine[4]={1,1,1,1},sine[4]={0,0,0,0};
    file.addVar("Times",ncChar,{time,dateLength}).putVar(times);
    NcVar xVar=file.addVar("x",ncDouble,x); xVar.putAtt("units","m"); xVar.putVar(xs);
    NcVar yVar=file.addVar("y",ncDouble,y); yVar.putAtt("units","m"); yVar.putVar(ys);
    NcVar windU=file.addVar("U10",ncFloat,{time,y,x}); windU.putAtt("units","m s-1"); windU.putVar(u);
    NcVar windV=file.addVar("V10",ncFloat,{time,y,x}); windV.putAtt("units","m s-1"); windV.putVar(v);
    file.addVar("COSALPHA",ncFloat,{y,x}).putVar(cosine); file.addVar("SINALPHA",ncFloat,{y,x}).putVar(sine);
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
    Array2<double> targetLon(3,3),targetLat(3,3);
    for (int j=0;j<3;j++) for (int i=0;i<3;i++) { targetLon(j,i)=-10+10.5*i; targetLat(j,i)=40+.5*j; }
    wave->regridBilinearGeographic(targetLon,targetLat);
    assert(wave->StokesU().Ny()==3 && wave->StokesU().Nz()==3);
    assert(std::abs(wave->StokesU()(0,1,1)-.25)<1.e-6);
    bool rejected=false; try { WeatherModelAdapterFactory::create("unknown",wrfFile); }
    catch (const std::runtime_error&) { rejected=true; } assert(rejected);
    std::string invalidFile="ww3-invalid-units.nc"; createWW3(invalidFile,"knots"); rejected=false;
    try { auto invalid=WaveModelAdapterFactory::create("WW3",invalidFile); invalid->process(); }
    catch (const std::runtime_error&) { rejected=true; } assert(rejected);
    std::string invalidTimeFile="ww3-invalid-time.nc"; createWW3(invalidTimeFile,"m s-1","months since 1968-05-23"); rejected=false;
    try { auto invalid=WaveModelAdapterFactory::create("WW3",invalidTimeFile); invalid->process(); }
    catch (const std::runtime_error&) { rejected=true; } assert(rejected);
#ifdef WACOMM_USE_PROJ
    std::string projectedFile="ww3-projected.nc"; createProjectedWW3(projectedFile);
    auto projectedWave=WaveModelAdapterFactory::create("WW3",projectedFile,"EPSG:3857"); projectedWave->process();
    Array2<double> projectedTargetLon(1,1),projectedTargetLat(1,1);
    projectedTargetLon(0,0)=.5; projectedTargetLat(0,0)=.5;
    projectedWave->regridBilinearProjected(projectedTargetLon,projectedTargetLat,"EPSG:3857");
    assert(std::abs(projectedWave->StokesU()(0,0,0)-1.49996)<1.e-4);
    std::string projectedWrfFile="wrf-projected.nc"; createProjectedWRF(projectedWrfFile);
    auto projectedWeather=WeatherModelAdapterFactory::create("WRF",projectedWrfFile,"EPSG:3857"); projectedWeather->process();
    projectedWeather->regridBilinearProjected(projectedTargetLon,projectedTargetLat,"EPSG:3857");
    assert(std::abs(projectedWeather->WindU10()(0,0,0)-1.49996)<1.e-4);
    std::remove(projectedFile.c_str()); std::remove(projectedWrfFile.c_str());
#endif
    std::remove(wrfFile.c_str()); std::remove(ww3File.c_str()); std::remove(invalidFile.c_str());
    std::remove(invalidTimeFile.c_str());
}
