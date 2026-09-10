#include "WRFAdapter.hpp"
#include "../EnvironmentalMetadata.hpp"
#include "../JulianDate.hpp"
#include <cstdio>
#include <stdexcept>
#include <vector>

WRFAdapter::WRFAdapter(std::string &fileName,const std::string& sourceCrs): fileName(fileName),sourceCrs(sourceCrs) {}

void WRFAdapter::process() {
    NcFile file(fileName,NcFile::read);
    NcVar u=file.getVar("U10"),v=file.getVar("V10");
    NcVar lon=file.getVar(sourceCrs.empty() ? "XLONG" : "x"),lat=file.getVar(sourceCrs.empty() ? "XLAT" : "y");
    if (u.isNull() || v.isNull() || lon.isNull() || lat.isNull())
        throw std::runtime_error(sourceCrs.empty() ? "WRF input requires U10, V10, XLONG, and XLAT" :
                                 "Projected WRF input requires U10, V10, x, and y");
    EnvironmentalMetadata::requireVelocity(u,"WRF U10");
    EnvironmentalMetadata::requireVelocity(v,"WRF V10");
    if (sourceCrs.empty()) {
        EnvironmentalMetadata::requireLongitude(lon,"WRF XLONG");
        EnvironmentalMetadata::requireLatitude(lat,"WRF XLAT");
    } else {
        EnvironmentalMetadata::requireProjectedCoordinate(lon,"WRF x");
        EnvironmentalMetadata::requireProjectedCoordinate(lat,"WRF y");
    }
    auto dims=u.getDims();
    if (dims.size()!=3 || v.getDims().size()!=3 || dims[0].getSize()!=v.getDim(0).getSize() ||
        dims[1].getSize()!=v.getDim(1).getSize() || dims[2].getSize()!=v.getDim(2).getSize())
        throw std::runtime_error("WRF U10 and V10 must use common [Time,south_north,west_east] dimensions");
    size_t nt=dims[0].getSize(),eta=dims[1].getSize(),xi=dims[2].getSize();
    if (nt==0 || eta<2 || xi<2) throw std::runtime_error("WRF input does not contain a usable grid");
    Time().Allocate(nt); Lon().Allocate(eta,xi); Lat().Allocate(eta,xi);
    WindU10().Allocate(nt,eta,xi); WindV10().Allocate(nt,eta,xi);
    std::vector<float> gridU(nt*eta*xi),gridV(gridU.size()); u.getVar(gridU.data()); v.getVar(gridV.data());
    NcVar cosine=file.getVar("COSALPHA"),sine=file.getVar("SINALPHA");
    if (cosine.isNull() || sine.isNull())
        throw std::runtime_error("WRF input requires COSALPHA and SINALPHA to rotate grid-relative U10/V10 to east/north");
    std::vector<float> cosValues(eta*xi),sinValues(eta*xi);
    auto compatibleRotation=[eta,xi](const NcVar& value) {
        return (value.getDimCount()==3 && value.getDim(0).getSize()>0 &&
                value.getDim(1).getSize()==eta && value.getDim(2).getSize()==xi) ||
               (value.getDimCount()==2 && value.getDim(0).getSize()==eta && value.getDim(1).getSize()==xi);
    };
    if (!compatibleRotation(cosine) || !compatibleRotation(sine) || cosine.getDimCount()!=sine.getDimCount())
        throw std::runtime_error("WRF COSALPHA/SINALPHA dimensions are incompatible with U10/V10");
    if (cosine.getDimCount()==3) {
        cosine.getVar({0,0,0},{1,eta,xi},cosValues.data()); sine.getVar({0,0,0},{1,eta,xi},sinValues.data());
    } else { cosine.getVar(cosValues.data()); sine.getVar(sinValues.data()); }
    for (int t=0;t<nt;t++) for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
        size_t horizontal=(size_t)j*xi+i,index=(size_t)t*eta*xi+horizontal;
        WindU10()(t,j,i)=gridU[index]*cosValues[horizontal]-gridV[index]*sinValues[horizontal];
        WindV10()(t,j,i)=gridV[index]*cosValues[horizontal]+gridU[index]*sinValues[horizontal];
    }
    if (!sourceCrs.empty() && lon.getDimCount()==1 && lat.getDimCount()==1 &&
        lon.getDim(0).getSize()==xi && lat.getDim(0).getSize()==eta) {
        std::vector<double> x(xi),y(eta); lon.getVar(x.data()); lat.getVar(y.data());
        for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) { Lon()(j,i)=x[i]; Lat()(j,i)=y[j]; }
    } else if (lon.getDimCount()==3 && lat.getDimCount()==3 && lon.getDim(0).getSize()>0 && lat.getDim(0).getSize()>0 &&
        lon.getDim(1).getSize()==eta && lon.getDim(2).getSize()==xi &&
        lat.getDim(1).getSize()==eta && lat.getDim(2).getSize()==xi) {
        lon.getVar({0,0,0},{1,eta,xi},Lon()()); lat.getVar({0,0,0},{1,eta,xi},Lat()());
    } else if (lon.getDimCount()==2 && lat.getDimCount()==2 &&
               lon.getDim(0).getSize()==eta && lon.getDim(1).getSize()==xi &&
               lat.getDim(0).getSize()==eta && lat.getDim(1).getSize()==xi) {
        lon.getVar(Lon()()); lat.getVar(Lat()());
    } else throw std::runtime_error("WRF XLONG and XLAT dimensions are incompatible with U10/V10");
    NcVar times=file.getVar("Times"),numeric=file.getVar("time");
    if (!numeric.isNull() && numeric.getDimCount()==1 && numeric.getDim(0).getSize()==nt)
        EnvironmentalMetadata::readCfTime(numeric,Time(),"WRF time");
    else if (!times.isNull() && times.getDimCount()==2 && times.getDim(0).getSize()==nt) {
        size_t length=times.getDim(1).getSize(); std::vector<char> values(nt*length); times.getVar(values.data());
        for (int t=0;t<nt;t++) {
            std::string stamp(values.data()+t*length,length);
            int year,month,day,hour,minute,second;
            if (std::sscanf(stamp.c_str(),"%d-%d-%d_%d:%d:%d",&year,&month,&day,&hour,&minute,&second)!=6)
                throw std::runtime_error("WRF Times must use YYYY-MM-DD_HH:MM:SS");
            Calendar calendar(year,month,day,hour,0,0);
            Time()(t)=JulianDate::toModJulian(calendar)*86400.0+minute*60.0+second;
        }
    } else throw std::runtime_error("WRF input requires absolute numeric time or Times(Time,DateStrLen)");
    for (int t=1;t<nt;t++) if (Time()(t)<=Time()(t-1)) throw std::runtime_error("WRF time must be strictly chronological");
}
