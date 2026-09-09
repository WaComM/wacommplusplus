#include "WaveModelAdapter.hpp"
#include "EnvironmentalRegridder.hpp"
#include <cmath>
#include <stdexcept>

Array1<double>& WaveModelAdapter::Time() { return time; }
Array2<double>& WaveModelAdapter::Lon() { return lon; }
Array2<double>& WaveModelAdapter::Lat() { return lat; }
Array3<float>& WaveModelAdapter::StokesU() { return stokesU; }
Array3<float>& WaveModelAdapter::StokesV() { return stokesV; }

void WaveModelAdapter::regridBilinearGeographic(const Array2<double>& targetLon,const Array2<double>& targetLat) {
    Array3<float> u=EnvironmentalRegridder::bilinearGeographic(Lon(),Lat(),StokesU(),targetLon,targetLat);
    Array3<float> v=EnvironmentalRegridder::bilinearGeographic(Lon(),Lat(),StokesV(),targetLon,targetLat);
    Lon().Deallocate(); Lon().Allocate(targetLon.Nx(),targetLon.Ny()); Lon().Load(targetLon());
    Lat().Deallocate(); Lat().Allocate(targetLat.Nx(),targetLat.Ny()); Lat().Load(targetLat());
    StokesU().Deallocate(); StokesU().Allocate(u.Nx(),u.Ny(),u.Nz()); StokesU().Load(u());
    StokesV().Deallocate(); StokesV().Allocate(v.Nx(),v.Ny(),v.Nz()); StokesV().Load(v());
}

void WaveModelAdapter::regridBilinearCurvilinearGeographic(const Array2<double>& targetLon,const Array2<double>& targetLat) {
    Array3<float> u=EnvironmentalRegridder::bilinearCurvilinearGeographic(Lon(),Lat(),StokesU(),targetLon,targetLat);
    Array3<float> v=EnvironmentalRegridder::bilinearCurvilinearGeographic(Lon(),Lat(),StokesV(),targetLon,targetLat);
    Lon().Deallocate(); Lon().Allocate(targetLon.Nx(),targetLon.Ny()); Lon().Load(targetLon());
    Lat().Deallocate(); Lat().Allocate(targetLat.Nx(),targetLat.Ny()); Lat().Load(targetLat());
    StokesU().Deallocate(); StokesU().Allocate(u.Nx(),u.Ny(),u.Nz()); StokesU().Load(u());
    StokesV().Deallocate(); StokesV().Allocate(v.Nx(),v.Ny(),v.Nz()); StokesV().Load(v());
}

void WaveModelAdapter::appendBoundaryRecord(WaveModelAdapter &adapter, int record, bool prepend) {
    if (record<0 || record>=adapter.Time().Nx()) throw std::runtime_error("Wave boundary record is out of range");
    size_t eta=Lon().Nx(),xi=Lon().Ny(),oldTime=Time().Nx(),newTime=oldTime+1;
    if (eta!=adapter.Lon().Nx() || xi!=adapter.Lon().Ny()) throw std::runtime_error("Adjacent wave files have incompatible grids");
    for (int j=0;j<eta;j++) for (int i=0;i<xi;i++)
        if (std::abs(Lon()(j,i)-adapter.Lon()(j,i))>1.e-10 || std::abs(Lat()(j,i)-adapter.Lat()(j,i))>1.e-10)
            throw std::runtime_error("Adjacent wave files have incompatible coordinates");
    Array1<double> newTimes(newTime); Array3<float> u(newTime,eta,xi),v(newTime,eta,xi);
    for (int t=0;t<newTime;t++) {
        bool boundary=prepend ? t==0 : t==(int)oldTime; int source=prepend ? t-1 : t;
        newTimes(t)=boundary ? adapter.Time()(record) : Time()(source);
        for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
            u(t,j,i)=boundary ? adapter.StokesU()(record,j,i) : StokesU()(source,j,i);
            v(t,j,i)=boundary ? adapter.StokesV()(record,j,i) : StokesV()(source,j,i);
        }
    }
    for (int t=1;t<newTime;t++) if (newTimes(t)<=newTimes(t-1)) throw std::runtime_error("Wave time must be chronological");
    Time().Reallocate(newTime); Time().Load(newTimes());
    StokesU().Deallocate(); StokesU().Allocate(newTime,eta,xi); StokesU().Load(u());
    StokesV().Deallocate(); StokesV().Allocate(newTime,eta,xi); StokesV().Load(v());
}
