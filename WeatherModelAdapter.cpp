#include "WeatherModelAdapter.hpp"
#include "EnvironmentalRegridder.hpp"
#include <cmath>
#include <stdexcept>

Array1<double>& WeatherModelAdapter::Time() { return _data.time; }
Array2<double>& WeatherModelAdapter::Lon() { return _data.lon; }
Array2<double>& WeatherModelAdapter::Lat() { return _data.lat; }
Array3<float>& WeatherModelAdapter::WindU10() { return _data.windU10; }
Array3<float>& WeatherModelAdapter::WindV10() { return _data.windV10; }

void WeatherModelAdapter::regridBilinearGeographic(const Array2<double>& targetLon,const Array2<double>& targetLat) {
    Array3<float> u=EnvironmentalRegridder::bilinearGeographic(Lon(),Lat(),WindU10(),targetLon,targetLat);
    Array3<float> v=EnvironmentalRegridder::bilinearGeographic(Lon(),Lat(),WindV10(),targetLon,targetLat);
    Lon().Deallocate(); Lon().Allocate(targetLon.Nx(),targetLon.Ny()); Lon().Load(targetLon());
    Lat().Deallocate(); Lat().Allocate(targetLat.Nx(),targetLat.Ny()); Lat().Load(targetLat());
    WindU10().Deallocate(); WindU10().Allocate(u.Nx(),u.Ny(),u.Nz()); WindU10().Load(u());
    WindV10().Deallocate(); WindV10().Allocate(v.Nx(),v.Ny(),v.Nz()); WindV10().Load(v());
}

void WeatherModelAdapter::appendBoundaryRecord(WeatherModelAdapter &adapter, int record, bool prepend) {
    if (record<0 || record>=adapter.Time().Nx()) throw std::runtime_error("Weather boundary record is out of range");
    size_t eta=Lon().Nx(),xi=Lon().Ny(),oldTime=Time().Nx(),newTime=oldTime+1;
    if (eta!=adapter.Lon().Nx() || xi!=adapter.Lon().Ny())
        throw std::runtime_error("Adjacent weather files have incompatible grids");
    for (int j=0;j<eta;j++) for (int i=0;i<xi;i++)
        if (std::abs(Lon()(j,i)-adapter.Lon()(j,i))>1.e-10 || std::abs(Lat()(j,i)-adapter.Lat()(j,i))>1.e-10)
            throw std::runtime_error("Adjacent weather files have incompatible coordinates");
    Array1<double> time(newTime); Array3<float> u(newTime,eta,xi),v(newTime,eta,xi);
    for (int t=0;t<newTime;t++) {
        bool boundary=prepend ? t==0 : t==(int)oldTime; int source=prepend ? t-1 : t;
        time(t)=boundary ? adapter.Time()(record) : Time()(source);
        for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
            u(t,j,i)=boundary ? adapter.WindU10()(record,j,i) : WindU10()(source,j,i);
            v(t,j,i)=boundary ? adapter.WindV10()(record,j,i) : WindV10()(source,j,i);
        }
    }
    for (int t=1;t<newTime;t++) if (time(t)<=time(t-1)) throw std::runtime_error("Weather time must be chronological");
    Time().Reallocate(newTime); Time().Load(time());
    WindU10().Deallocate(); WindU10().Allocate(newTime,eta,xi); WindU10().Load(u());
    WindV10().Deallocate(); WindV10().Allocate(newTime,eta,xi); WindV10().Load(v());
}
