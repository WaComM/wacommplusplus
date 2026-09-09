#include "WW3Adapter.hpp"
#include <stdexcept>
#include <vector>

WW3Adapter::WW3Adapter(std::string &fileName): fileName(fileName) {}

NcVar WW3Adapter::variable(NcFile &file, const std::vector<std::string>& names) {
    for (const auto& name:names) { NcVar value=file.getVar(name); if (!value.isNull()) return value; }
    std::string aliases; for (const auto& name:names) aliases+=(aliases.empty() ? "" : ", ")+name;
    throw std::runtime_error("WW3 input is missing required variable (aliases: " + aliases + ")");
}

void WW3Adapter::process() {
    NcFile file(fileName,NcFile::read);
    NcVar time=variable(file,{"time"}),lon=variable(file,{"longitude","lon"}),lat=variable(file,{"latitude","lat"});
    NcVar u=variable(file,{"uuss","ust","stokes_u","eastward_surface_stokes_drift",
                                "sea_surface_wave_stokes_drift_x_velocity"});
    NcVar v=variable(file,{"vuss","vst","stokes_v","northward_surface_stokes_drift",
                                "sea_surface_wave_stokes_drift_y_velocity"});
    auto dims=u.getDims();
    if (dims.size()!=3 || v.getDims().size()!=3 || dims[0].getSize()!=v.getDim(0).getSize() ||
        dims[1].getSize()!=v.getDim(1).getSize() || dims[2].getSize()!=v.getDim(2).getSize())
        throw std::runtime_error("WW3 Stokes components must use common [time,latitude,longitude] dimensions");
    size_t nt=dims[0].getSize(),eta=dims[1].getSize(),xi=dims[2].getSize();
    if (nt==0 || eta<2 || xi<2 || time.getDimCount()!=1 || time.getDim(0).getSize()!=nt)
        throw std::runtime_error("WW3 input does not contain a usable time/grid axis");
    Time().Allocate(nt); Lon().Allocate(eta,xi); Lat().Allocate(eta,xi);
    StokesU().Allocate(nt,eta,xi); StokesV().Allocate(nt,eta,xi);
    time.getVar(Time()()); u.getVar(StokesU()()); v.getVar(StokesV()());
    if (lon.getDimCount()==1 && lat.getDimCount()==1 && lon.getDim(0).getSize()==xi && lat.getDim(0).getSize()==eta) {
        std::vector<double> x(xi),y(eta); lon.getVar(x.data()); lat.getVar(y.data());
        for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) { Lon()(j,i)=x[i]>180 ? x[i]-360 : x[i]; Lat()(j,i)=y[j]; }
    } else if (lon.getDimCount()==2 && lat.getDimCount()==2 && lon.getDim(0).getSize()==eta && lon.getDim(1).getSize()==xi) {
        lon.getVar(Lon()()); lat.getVar(Lat()());
    } else throw std::runtime_error("WW3 longitude/latitude dimensions are incompatible with Stokes drift");
    for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) if (Lon()(j,i)>180) Lon()(j,i)-=360;
    for (int t=1;t<nt;t++) if (Time()(t)<=Time()(t-1)) throw std::runtime_error("WW3 time must be strictly chronological");
}
