#include "../OceanModelAdapters/WacommAdapter.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>

class FixtureAdapter: public OceanModelAdapter {
public:
    void process() override {}
};

int main() {
    const string fileName="native-adapter-test.nc";
    FixtureAdapter fixture;
    fixture.OceanTime().Allocate(2); fixture.OceanTime()(0)=0; fixture.OceanTime()(1)=3600;
    fixture.SRho().Allocate(2,-1); fixture.SRho()(-1)=-.75; fixture.SRho()(0)=-.25;
    fixture.SW().Allocate(3,-2); fixture.SW()(-2)=-1; fixture.SW()(-1)=-.5; fixture.SW()(0)=0;
    fixture.Mask().Allocate(2,2); fixture.Mask()=1.0;
    fixture.Lon().Allocate(2,2); fixture.Lat().Allocate(2,2); fixture.H().Allocate(2,2);
    for (int j=0;j<2;j++) for (int i=0;i<2;i++) {
        fixture.Lon()(j,i)=10+i; fixture.Lat()(j,i)=40+j; fixture.H()(j,i)=100;
    }
    fixture.Zeta().Allocate(2,2,2); fixture.Zeta()=0.0f;
    fixture.U().Allocate(2,2,2,2,0,-1,0,0); fixture.U()=.25f;
    fixture.V().Allocate(2,2,2,2,0,-1,0,0); fixture.V()=-.1f;
    fixture.W().Allocate(2,3,2,2,0,-2,0,0); fixture.W()=0.0f;
    fixture.AKT().Allocate(2,3,2,2,0,-2,0,0); fixture.AKT()=.01f;
    string output=fileName;
    fixture.saveAsNetCDF(output);

    WacommAdapter adapter(output);
    adapter.process();
    assert(adapter.OceanTime().Nx()==2 && adapter.Mask().Nx()==2 && adapter.Mask().Ny()==2);
    assert(adapter.OceanTime()(1)==3600 && adapter.SW()(-2)==-1);
    assert(adapter.U()(1,-1,1,1)==.25f && adapter.V()(0,0,0,0)==-.1f);
    assert(adapter.AKT()(1,0,1,1)==.01f);
    double particleDepth,particleLat,particleLon;
    adapter.kji2deplatlon(-.5,.5,.5,particleDepth,particleLat,particleLon);
    assert(particleDepth==-25 && particleLat==40.5 && particleLon==10.5);
    adapter.kji2deplatlon(-.5,-.2,.5,particleDepth,particleLat,particleLon);
    assert(particleDepth==1e37 && particleLat==1e37 && particleLon==1e37);
    double k,j,i;
    adapter.deplatlon2kji(0,40.5,10.5,k,j,i);
    assert(k==0);
    adapter.deplatlon2kji(200,40.5,10.5,k,j,i);
    assert(k==-1);
    string nextFile="native-adapter-next-test.nc";
    {
        std::ifstream input(fileName,std::ios::binary);
        std::ofstream output(nextFile,std::ios::binary);
        output << input.rdbuf();
    }
    {
        NcFile file(nextFile,NcFile::write);
        double times[2]={5400,7200}; file.getVar("ocean_time").putVar(times);
    }
    WacommAdapter next(nextFile); next.process();
    adapter.appendBoundaryRecord(next,0,false);
    assert(adapter.OceanTime().Nx()==3 && adapter.OceanTime()(2)==5400);
    assert(adapter.U()(2,-1,1,1)==.25f);
    std::remove(fileName.c_str());
    std::remove(nextFile.c_str());
}
