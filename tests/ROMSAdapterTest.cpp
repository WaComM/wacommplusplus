#include "../OceanModelAdapters/ROMSAdapter.hpp"
#include "AdapterRestartEquivalence.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
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
        file.addVar("u",ncFloat,uDims);
        file.addVar("v",ncFloat,vDims);
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
