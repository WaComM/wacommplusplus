#include "../OceanModelAdapters/ROMSAdapter.hpp"
#include "AdapterRestartEquivalence.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include "../OceanModelAdapters/WacommAdapter.hpp"
#include <vector>

using namespace netCDF;

int main() {
    const string fileName="roms-adapter-test.nc";
    {
        NcFile file(fileName,NcFile::replace,NcFile::nc4);
        NcDim time=file.addDim("ocean_time",2),sRho=file.addDim("s_rho",2),sW=file.addDim("s_w",3);
        NcDim etaRho=file.addDim("eta_rho",2),xiRho=file.addDim("xi_rho",3);
        NcDim etaU=file.addDim("eta_u",2),xiU=file.addDim("xi_u",2);
        NcDim etaV=file.addDim("eta_v",1),xiV=file.addDim("xi_v",3);
        vector<NcDim> rhoDims{etaRho,xiRho},uDims{time,sRho,etaU,xiU};
        vector<NcDim> vDims{time,sRho,etaV,xiV},wDims{time,sW,etaRho,xiRho};
        vector<NcDim> surfaceDims{time,etaRho,xiRho};
        file.addVar("mask_rho",ncDouble,rhoDims);
        file.addVar("mask_u",ncDouble,{etaU,xiU});
        file.addVar("mask_v",ncDouble,{etaV,xiV});
        file.addVar("lat_rho",ncDouble,rhoDims);
        file.addVar("lon_rho",ncDouble,rhoDims);
        file.addVar("lat_v",ncDouble,{etaV,xiV});
        file.addVar("lon_u",ncDouble,{etaU,xiU});
        file.addVar("h",ncDouble,rhoDims);
        file.addVar("u",ncFloat,uDims).putAtt("units","meter second-1");
        file.addVar("v",ncFloat,vDims).putAtt("units","meter second-1");
        NcVar angle=file.addVar("angle",ncDouble,rhoDims);
        angle.putAtt("units","radians");
        double angles[6]={}; angle.putVar(angles);
        file.addVar("w",ncFloat,wDims);
        file.addVar("AKt",ncFloat,wDims);
        file.addVar("s_w",ncDouble,sW);
        file.addVar("s_rho",ncDouble,sRho);
        file.addVar("ocean_time",ncDouble,time);
        file.addVar("zeta",ncFloat,surfaceDims);

        double onesRho[6]={1,1,1,1,1,1},onesU[4]={1,1,1,1},onesV[3]={1,1,1};
        double lat[6]={40,40,40,41,41,41},lon[6]={10,11,12,10,11,12};
        double depth[6]={100,100,100,100,100,100},times[2]={0,3600};
        double sRhoValues[2]={-.75,-.25},sWValues[3]={-1,-.5,0};
        float uValues[16],vValues[12],wValues[36]={},zetaValues[12]={};
        for (int t=0;t<2;t++) for (int k=0;k<2;k++) for (int j=0;j<2;j++) {
            uValues[((t*2+k)*2+j)*2]=2;
            uValues[((t*2+k)*2+j)*2+1]=4;
        }
        for (float &value:vValues) value=6;
        file.getVar("mask_rho").putVar(onesRho); file.getVar("mask_u").putVar(onesU);
        file.getVar("mask_v").putVar(onesV); file.getVar("lat_rho").putVar(lat);
        file.getVar("lon_rho").putVar(lon); file.getVar("lat_v").putVar(lat);
        file.getVar("lon_u").putVar(lon); file.getVar("h").putVar(depth);
        file.getVar("u").putVar(uValues); file.getVar("v").putVar(vValues);
        file.getVar("w").putVar(wValues); file.getVar("AKt").putVar(wValues);
        file.getVar("s_w").putVar(sWValues); file.getVar("s_rho").putVar(sRhoValues);
        file.getVar("ocean_time").putVar(times); file.getVar("zeta").putVar(zetaValues);
    }

    string input=fileName;
    ROMSAdapter adapter(input);
    adapter.process();
    assertAdapterRestartEquivalence(adapter);
    assert(adapter.OceanTime()(0)==0 && adapter.OceanTime()(1)==3600);
    for (int t=0;t<2;t++) for (int k=-1;k<=0;k++) for (int j=0;j<2;j++) {
        assert(std::abs(adapter.U()(t,k,j,0)-2)<1e-6);
        assert(std::abs(adapter.U()(t,k,j,1)-3)<1e-6);
        assert(std::abs(adapter.U()(t,k,j,2)-4)<1e-6);
        for (int i=0;i<3;i++) assert(std::abs(adapter.V()(t,k,j,i)-6)<1e-6);
    }
    auto expectRejected=[&]() {
        bool rejected=false;
        try { ROMSAdapter invalid(input); invalid.process(); }
        catch (const std::runtime_error&) { rejected=true; }
        assert(rejected);
    };
    const double pi=std::acos(-1.0);
    for (double theta:{0.0,.5*pi,-.5*pi,pi,.25*pi}) {
        {
            NcFile file(fileName,NcFile::write);
            double angles[6]; for (double &value:angles) value=theta;
            file.getVar("angle").putVar(angles);
        }
        ROMSAdapter rotated(input); rotated.process();
        assertAdapterRestartEquivalence(rotated);
        assertAdapterRestartEquivalence(rotated,true);
        for (int t=0;t<2;t++) for (int k=-1;k<=0;k++) for (int j=0;j<2;j++) for (int i=0;i<3;i++) {
            double u=2+i,v=6;
            assert(std::abs(rotated.U()(t,k,j,i)-(u*std::cos(theta)-v*std::sin(theta)))<1e-6);
            assert(std::abs(rotated.V()(t,k,j,i)-(u*std::sin(theta)+v*std::cos(theta)))<1e-6);
            assert(std::abs(std::hypot(rotated.U()(t,k,j,i),rotated.V()(t,k,j,i))-std::hypot(u,v))<1e-6);
        }
        string nativeFile="roms-rotated-native-test.nc";
        rotated.saveAsNetCDF(nativeFile);
        {
            NcFile file(nativeFile,NcFile::read);
            string basis; file.getVar("u").getAtt("standard_name").getValues(basis);
            assert(basis=="eastward_sea_water_velocity");
        }
        WacommAdapter native(nativeFile); native.process();
        assertAdapterRestartEquivalence(native);
        for (int t=0;t<2;t++) for (int k=-1;k<=0;k++) for (int j=0;j<2;j++) for (int i=0;i<3;i++) {
            assert(native.U()(t,k,j,i)==rotated.U()(t,k,j,i));
            assert(native.V()(t,k,j,i)==rotated.V()(t,k,j,i));
        }
        std::remove(nativeFile.c_str());
    }
    {
        NcFile file(fileName,NcFile::write);
        double angles[6]={0,.5*pi,-.5*pi,pi,.25*pi,std::numeric_limits<double>::quiet_NaN()};
        double mask[6]={1,1,1,1,1,0};
        file.getVar("angle").putVar(angles); file.getVar("mask_rho").putVar(mask);
    }
    {
        ROMSAdapter varied(input); varied.process();
        assert(std::abs(varied.U()(0,-1,0,1)+6)<1e-6);
        assert(std::abs(varied.V()(0,-1,0,1)-3)<1e-6);
        assert(varied.U()(0,-1,1,2)==0 && varied.V()(0,-1,1,2)==0);
    }
    {
        NcFile file(fileName,NcFile::write);
        double mask[6]={1,1,1,1,1,1}; file.getVar("mask_rho").putVar(mask);
    }
    expectRejected();
    for (double invalid:{std::numeric_limits<double>::infinity(),NC_FILL_DOUBLE,-9999.0}) {
        {
            NcFile file(fileName,NcFile::write);
            double angles[6]={0,0,0,0,0,invalid};
            file.getVar("angle").putAtt("missing_value",ncDouble,-9999.0);
            file.getVar("angle").putVar(angles);
        }
        expectRejected();
    }
    {
        NcFile file(fileName,NcFile::write);
        double angles[6]={}; file.getVar("angle").putVar(angles);
        file.getVar("angle").putAtt("units","degrees");
    }
    expectRejected();
    {
        NcFile file(fileName,NcFile::write);
        file.getVar("angle").putAtt("units","radians");
        file.getVar("angle").rename("saved_angle");
    }
    expectRejected();
    {
        NcFile file(fileName,NcFile::write);
        file.getVar("u").putAtt("standard_name","eastward_sea_water_velocity");
    }
    expectRejected();
    {
        NcFile file(fileName,NcFile::write);
        file.getVar("v").putAtt("standard_name","northward_sea_water_velocity");
    }
    {
        ROMSAdapter earth(input); earth.process();
        assert(earth.U()(0,-1,0,1)==3 && earth.V()(0,-1,0,1)==6);
    }
    {
        NcFile file(fileName,NcFile::write);
        file.getVar("saved_angle").rename("angle");
        double angles[6]={pi,pi,pi,pi,pi,pi}; file.getVar("angle").putVar(angles);
    }
    {
        ROMSAdapter earth(input); earth.process();
        assert(earth.U()(0,-1,0,1)==3 && earth.V()(0,-1,0,1)==6);
    }
    {
        NcFile file(fileName,NcFile::write);
        file.getVar("u").putAtt("standard_name",""); file.getVar("v").putAtt("standard_name","");
        file.getVar("angle").rename("saved_angle");
        file.addVar("angle",ncDouble,{file.getDim("xi_rho"),file.getDim("eta_rho")}).putAtt("units","radians");
    }
    expectRejected();
    {
        NcFile file(fileName,NcFile::write);
        file.getVar("angle").rename("bad_angle"); file.getVar("saved_angle").rename("angle");
        double angles[6]={}; file.getVar("angle").putVar(angles);
        file.getVar("u").putAtt("units","centimeter second-1");
    }
    expectRejected();
    {
        NcFile file(fileName,NcFile::write);
        file.getVar("u").putAtt("units","meter second-1");
        float invalid=std::numeric_limits<float>::quiet_NaN();
        file.getVar("u").putVar({0,0,0,0},{1,1,1,1},&invalid);
    }
    expectRejected();
    {
        NcFile file(fileName,NcFile::write);
        float value=2; file.getVar("u").putVar({0,0,0,0},{1,1,1,1},&value);
    }
    const string nextFile="roms-adapter-next-test.nc";
    {
        std::ifstream input(fileName,std::ios::binary);
        std::ofstream output(nextFile,std::ios::binary);
        output << input.rdbuf();
    }
    {
        NcFile file(nextFile,NcFile::write);
        double times[2]={5400,7200}; file.getVar("ocean_time").putVar(times);
        float values[16]; for (float &value:values) value=8;
        file.getVar("u").putVar(values);
    }
    string nextInput=nextFile;
    ROMSAdapter next(nextInput); next.process();
    adapter.appendBoundaryRecord(next,0,false);
    assert(adapter.OceanTime().Nx()==3 && adapter.OceanTime()(2)==5400);
    assert(adapter.U()(2,-1,0,1)==8);
    std::remove(fileName.c_str());
    std::remove(nextFile.c_str());
}
