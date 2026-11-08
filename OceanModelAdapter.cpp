//
// Created by Raffaele Montella on 10/12/20.
//

#include "OceanModelAdapter.hpp"
#include "NumericalHelpers.hpp"
#include <math.h>
#include <cmath>
#include <stdexcept>

OceanModelAdapter::OceanModelAdapter() {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));
}

Array1<double> &OceanModelAdapter::OceanTime() { return _data.oceanTime; }
Array1<double> &OceanModelAdapter::SRho() { return _data.sRho; }
Array1<double> &OceanModelAdapter::SW() { return _data.sW; }
Array2<double> &OceanModelAdapter::Mask() { return _data.mask; }
Array2<double> &OceanModelAdapter::Lon() { return _data.lon; }
Array2<double> &OceanModelAdapter::Lat() { return _data.lat; }
Array2<double> &OceanModelAdapter::LonRad() { return _data.lonRad; }
Array2<double> &OceanModelAdapter::LatRad() { return _data.latRad; }
Array1<double> &OceanModelAdapter::DepthIntervals() { return _data.depthIntervals; }
Array2<double> &OceanModelAdapter::H() { return _data.h; }
Array3<float> &OceanModelAdapter::Zeta() { return _data.zeta; }
Array4<float> &OceanModelAdapter::U() { return _data.u; }
Array4<float> &OceanModelAdapter::V() { return _data.v; }
Array4<float> &OceanModelAdapter::W() { return _data.w; }
Array4<float> &OceanModelAdapter::AKT() { return _data.akt; }

Array1<double> &OceanModelAdapter::Latitude(){ return latitude; }
Array1<double> &OceanModelAdapter::Longitude() { return longitude; }
Array1<double> &OceanModelAdapter::Depth() { return depth; }



void OceanModelAdapter::saveAsNetCDF(std::string &fileName) {

    size_t ocean_time=_data.oceanTime.Nx();
    size_t s_rho=_data.sRho.Nx();
    size_t s_w=_data.sW.Nx();
    size_t eta_rho=_data.mask.Nx();
    size_t xi_rho=_data.mask.Ny();

    LOG4CPLUS_DEBUG(logger,"Saving in: " << fileName);

    // Open the file for read access
    netCDF::NcFile dataFile(fileName, NcFile::replace,NcFile::nc4);

    NcDim oceanTimeDim = dataFile.addDim("ocean_time", ocean_time);
    NcDim sRhoDim = dataFile.addDim("s_rho", s_rho);
    NcDim sWDim = dataFile.addDim("s_w", s_w);
    NcDim etaRhoDim = dataFile.addDim("eta_rho", eta_rho);
    NcDim xiRhoDim = dataFile.addDim("xi_rho", xi_rho);

    NcVar oceanTimeVar = dataFile.addVar("ocean_time", ncDouble, oceanTimeDim);
    oceanTimeVar.putAtt("long_name","time since initialization");
    oceanTimeVar.putAtt("units","seconds since 1968-05-23 00:00:00 GMT");
    oceanTimeVar.putAtt("calendar","gregorian");
    oceanTimeVar.putAtt("field","time, scalar, series");
    oceanTimeVar.putAtt("_CoordinateAxisType","Time");
    oceanTimeVar.putVar(_data.oceanTime());

    NcVar sRhoVar = dataFile.addVar("s_rho", ncDouble, sRhoDim);
    sRhoVar.putAtt("long_name","S-coordinate at RHO-points");
    sRhoVar.putAtt("valid_min",ncDouble, -1.0);
    sRhoVar.putAtt("valid_max",ncDouble, 0.0);
    sRhoVar.putAtt("positive","up");
    sRhoVar.putAtt("standard_name","ocean_sigma_coordinates");
    sRhoVar.putAtt("field","s_rho, scalar");
    sRhoVar.putAtt("_CoordinateTransformType","Vertical");
    sRhoVar.putAtt("_CoordinateAxes","s_rho");
    sRhoVar.putAtt("_CoordinateAxisType","GeoZ");
    sRhoVar.putAtt("_CoordinateZisPositive","up");
    sRhoVar.putVar(_data.sRho());

    NcVar sWVar = dataFile.addVar("s_w", ncDouble, sWDim);
    sWVar.putAtt("long_name","S-coordinate at W-points");
    sWVar.putAtt("valid_min",ncDouble, -1.0);
    sWVar.putAtt("valid_max",ncDouble, 0.0);
    sWVar.putAtt("positive","up");
    sWVar.putAtt("standard_name","ocean_sigma_coordinates");
    sWVar.putAtt("field","s_w, scalar");
    sWVar.putAtt("_CoordinateTransformType","Vertical");
    sWVar.putAtt("_CoordinateAxes","s_w");
    sWVar.putAtt("_CoordinateAxisType","GeoZ");
    sWVar.putAtt("_CoordinateZisPositive","up");
    sWVar.putVar(_data.sW());

    vector<NcDim> etaRhoXiRhoDims;
    etaRhoXiRhoDims.push_back(etaRhoDim);
    etaRhoXiRhoDims.push_back(xiRhoDim);
    NcVar latRhoVar = dataFile.addVar("lat_rho", ncDouble, etaRhoXiRhoDims);
    latRhoVar.putAtt("long_name","latitude of rho-points");
    latRhoVar.putAtt("unit","degree_north");
    latRhoVar.putAtt("standard_name","latitude");
    latRhoVar.putAtt("field","lat_rho, scalar");
    latRhoVar.putAtt("_coordinateaxistype","lat");
    latRhoVar.putVar(_data.lat());

    NcVar lonRhoVar = dataFile.addVar("lon_rho", ncDouble, etaRhoXiRhoDims);
    lonRhoVar.putAtt("long_name","longitude of rho-points");
    lonRhoVar.putAtt("unit","degree_east");
    lonRhoVar.putAtt("standard_name","longitude");
    lonRhoVar.putAtt("field","lon_rho, scalar");
    lonRhoVar.putAtt("_coordinateaxistype","lon");
    lonRhoVar.putVar(_data.lon());

    NcVar maskRhoVar = dataFile.addVar("mask_rho", ncDouble, etaRhoXiRhoDims);
    maskRhoVar.putAtt("long_name","mask on RHO-points");
    maskRhoVar.putAtt("coordinates","lon_rho lat_rho");
    maskRhoVar.putAtt("units","1");
    maskRhoVar.putVar(_data.mask());

    NcVar hVar = dataFile.addVar("h", ncDouble, etaRhoXiRhoDims);
    hVar.putAtt("long_name","bathymetry at RHO-point");
    hVar.putAtt("units","meter");
    hVar.putAtt("coordinates","lon_rho lat_rho");
    hVar.putAtt("field","bath, scalar");
    hVar.putVar(_data.h());

    vector<NcDim> oceanTimeEtaRhoXiRhoDims;
    oceanTimeEtaRhoXiRhoDims.push_back(oceanTimeDim);
    oceanTimeEtaRhoXiRhoDims.push_back(etaRhoDim);
    oceanTimeEtaRhoXiRhoDims.push_back(xiRhoDim);

    NcVar varZeta = dataFile.addVar("zeta", ncFloat, oceanTimeEtaRhoXiRhoDims);
    varZeta.putAtt("long_name","free-surface");
    varZeta.putAtt("units","meter");
    varZeta.putAtt("time","ocean_time");
    varZeta.putAtt("coordinates","lon_rho lat_rho ocean_time");
    varZeta.putAtt("field","free-surface, scalar, series");
    varZeta.putAtt("_FillValue",ncFloat, 9.99999993e+36);
    varZeta.putVar(_data.zeta());

    vector<NcDim> oceanTimeSRhoEtaRhoXiRhoDims;
    oceanTimeSRhoEtaRhoXiRhoDims.push_back(oceanTimeDim);
    oceanTimeSRhoEtaRhoXiRhoDims.push_back(sRhoDim);
    oceanTimeSRhoEtaRhoXiRhoDims.push_back(etaRhoDim);
    oceanTimeSRhoEtaRhoXiRhoDims.push_back(xiRhoDim);

    NcVar uVar = dataFile.addVar("u", ncFloat, oceanTimeSRhoEtaRhoXiRhoDims);
    uVar.putAtt("long_name","u-momentum component at RHO-points");
    uVar.putAtt("units","meter second-1");
    uVar.putAtt("standard_name","eastward_sea_water_velocity");
    uVar.putAtt("grid","grid");
    uVar.putAtt("loction","face");
    uVar.putAtt("coordinates","lon_rho lat_rho s_rho ocean_time");
    uVar.putAtt("field","u-velocity, scalar, series");
    uVar.putAtt("time","ocean_time");
    uVar.putAtt("_FillValue",ncFloat, 9.99999993e+36);
    uVar.putVar(_data.u());

    NcVar vVar = dataFile.addVar("v", ncFloat, oceanTimeSRhoEtaRhoXiRhoDims);
    vVar.putAtt("long_name","v-momentum component at RHO-points");
    vVar.putAtt("units","meter second-1");
    vVar.putAtt("standard_name","northward_sea_water_velocity");
    vVar.putAtt("grid","grid");
    vVar.putAtt("loction","face");
    vVar.putAtt("coordinates","lon_rho lat_rho s_rho ocean_time");
    vVar.putAtt("field","v-velocity, scalar, series");
    vVar.putAtt("time","ocean_time");
    vVar.putAtt("_FillValue",ncFloat, 9.99999993e+36);
    vVar.putVar(_data.v());

    vector<NcDim> oceanTimeSWEtaRhoXiRhoDims;
    oceanTimeSWEtaRhoXiRhoDims.push_back(oceanTimeDim);
    oceanTimeSWEtaRhoXiRhoDims.push_back(sWDim);
    oceanTimeSWEtaRhoXiRhoDims.push_back(etaRhoDim);
    oceanTimeSWEtaRhoXiRhoDims.push_back(xiRhoDim);

    NcVar wVar = dataFile.addVar("w", ncFloat, oceanTimeSWEtaRhoXiRhoDims);
    wVar.putAtt("long_name","vertical momentum component");
    wVar.putAtt("units","meter second-1");
    wVar.putAtt("grid","grid");
    wVar.putAtt("loction","face");
    wVar.putAtt("coordinates","lon_rho lat_rho s_w ocean_time");
    wVar.putAtt("field","w-velocity, scalar, series");
    wVar.putAtt("time","ocean_time");
    wVar.putAtt("_FillValue",ncFloat, 9.99999993e+36);
    wVar.putVar(_data.w());

    NcVar aktVar = dataFile.addVar("akt", ncFloat, oceanTimeSWEtaRhoXiRhoDims);
    aktVar.putAtt("long_name","temperature vertical diffusion coefficient");
    aktVar.putAtt("units","meter2 second-1");
    aktVar.putAtt("grid","grid");
    aktVar.putAtt("location","face");
    aktVar.putAtt("coordinates","lon_rho lat_rho s_w ocean_time");
    aktVar.putAtt("field","AKt, scalar, series");
    aktVar.putAtt("time","ocean_time");
    aktVar.putVar(_data.akt());

}

oceanmodel_data *OceanModelAdapter::dataptr() {
    return &_data;
}

void OceanModelAdapter::appendBoundaryRecord(OceanModelAdapter &adapter, int record, bool prepend) {
    if (record<0 || record>=adapter.OceanTime().Nx())
        throw std::runtime_error("Adjacent forcing boundary record is out of range");
    if (_data.sRho.Nx()!=adapter.SRho().Nx() || _data.sW.Nx()!=adapter.SW().Nx() ||
        _data.mask.Nx()!=adapter.Mask().Nx() || _data.mask.Ny()!=adapter.Mask().Ny())
        throw std::runtime_error("Adjacent forcing files have incompatible grid dimensions");
    for (int k=-(int)_data.sRho.Nx()+1;k<=0;k++)
        if (std::abs(_data.sRho(k)-adapter.SRho()(k))>1e-12)
            throw std::runtime_error("Adjacent forcing files have incompatible vertical coordinates");
    for (int k=-(int)_data.sW.Nx()+1;k<=0;k++)
        if (std::abs(_data.sW(k)-adapter.SW()(k))>1e-12)
            throw std::runtime_error("Adjacent forcing files have incompatible vertical coordinates");
    for (int j=0;j<_data.mask.Nx();j++) for (int i=0;i<_data.mask.Ny();i++)
        if (std::abs(_data.lon(j,i)-adapter.Lon()(j,i))>1e-10 ||
            std::abs(_data.lat(j,i)-adapter.Lat()(j,i))>1e-10 ||
            std::abs(_data.h(j,i)-adapter.H()(j,i))>1e-8 || _data.mask(j,i)!=adapter.Mask()(j,i))
            throw std::runtime_error("Adjacent forcing files have incompatible horizontal grids");

    // Saved native windows can already contain the adjacent boundary record.
    int endpoint=prepend ? 0 : (int)_data.oceanTime.Nx()-1;
    if (endpoint>=0 && _data.oceanTime(endpoint)==adapter.OceanTime()(record)) {
        for (int j=0;j<_data.mask.Nx();j++) for (int i=0;i<_data.mask.Ny();i++) {
            if (_data.zeta(endpoint,j,i)!=adapter.Zeta()(record,j,i))
                throw std::runtime_error("Duplicate forcing boundary has conflicting surface elevation");
            for (int k=-(int)_data.sRho.Nx()+1;k<=0;k++) {
                if (_data.u(endpoint,k,j,i)!=adapter.U()(record,k,j,i) ||
                    _data.v(endpoint,k,j,i)!=adapter.V()(record,k,j,i))
                    throw std::runtime_error("Duplicate forcing boundary has conflicting horizontal velocity");
            }
            for (int k=-(int)_data.sW.Nx()+1;k<=0;k++) {
                if (_data.w(endpoint,k,j,i)!=adapter.W()(record,k,j,i) ||
                    _data.akt(endpoint,k,j,i)!=adapter.AKT()(record,k,j,i))
                    throw std::runtime_error("Duplicate forcing boundary has conflicting vertical fields");
            }
        }
        return;
    }

    size_t oldTime=_data.oceanTime.Nx(),newTime=oldTime+1,sRho=_data.sRho.Nx(),sW=_data.sW.Nx();
    size_t eta=_data.mask.Nx(),xi=_data.mask.Ny();
    Array1<double> oceanTime(newTime);
    Array3<float> zeta(newTime,eta,xi);
    Array4<float> u(newTime,sRho,eta,xi,0,-(int)sRho+1,0,0);
    Array4<float> v(newTime,sRho,eta,xi,0,-(int)sRho+1,0,0);
    Array4<float> w(newTime,sW,eta,xi,0,-(int)sW+1,0,0);
    Array4<float> akt(newTime,sW,eta,xi,0,-(int)sW+1,0,0);
    for (int t=0;t<newTime;t++) {
        bool boundary=prepend ? t==0 : t==(int)oldTime;
        int source=prepend ? t-1 : t;
        oceanTime(t)=boundary ? adapter.OceanTime()(record) : _data.oceanTime(source);
        for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
            zeta(t,j,i)=boundary ? adapter.Zeta()(record,j,i) : _data.zeta(source,j,i);
            for (int k=-(int)sRho+1;k<=0;k++) {
                u(t,k,j,i)=boundary ? adapter.U()(record,k,j,i) : _data.u(source,k,j,i);
                v(t,k,j,i)=boundary ? adapter.V()(record,k,j,i) : _data.v(source,k,j,i);
            }
            for (int k=-(int)sW+1;k<=0;k++) {
                w(t,k,j,i)=boundary ? adapter.W()(record,k,j,i) : _data.w(source,k,j,i);
                akt(t,k,j,i)=boundary ? adapter.AKT()(record,k,j,i) : _data.akt(source,k,j,i);
            }
        }
    }
    for (int t=1;t<newTime;t++) if (oceanTime(t)<=oceanTime(t-1))
        throw std::runtime_error("Adjacent forcing files do not form a strictly chronological time axis");
    _data.oceanTime.Reallocate(newTime); _data.oceanTime.Load(oceanTime());
    _data.zeta.Deallocate(); _data.zeta.Allocate(newTime,eta,xi); _data.zeta.Load(zeta());
    _data.u.Deallocate(); _data.u.Allocate(newTime,sRho,eta,xi,0,-(int)sRho+1,0,0); _data.u.Load(u());
    _data.v.Deallocate(); _data.v.Allocate(newTime,sRho,eta,xi,0,-(int)sRho+1,0,0); _data.v.Load(v());
    _data.w.Deallocate(); _data.w.Allocate(newTime,sW,eta,xi,0,-(int)sW+1,0,0); _data.w.Load(w());
    _data.akt.Deallocate(); _data.akt.Allocate(newTime,sW,eta,xi,0,-(int)sW+1,0,0); _data.akt.Load(akt());
}

void OceanModelAdapter::kji2deplatlon(double k, double j, double i, double &dep, double &lat, double &lon) {
    // Get the integer part and the fraction part of particle k
    auto kI=NumericalHelpers::upperVerticalLevel(k);
    double kF=NumericalHelpers::lowerVerticalWeight(k);

    // Get the integer part and the fraction part of particle j
    auto jI=NumericalHelpers::horizontalCell(j); double jF=j-jI;

    // Get the integer part and the fraction part of particle i
    auto iI=NumericalHelpers::horizontalCell(i); double iF=i-iI;

    // Check if the source must be skipped
    if (!NumericalHelpers::validInterpolationCell(jI,iI,_data.mask.Nx(),_data.mask.Ny()) ||
        kI>0 || kI<=-(int)_data.sW.Nx()+1) {
        lon = 1e37;
        lat = 1e37;
        dep = 1e37;
        return;
    }

    // Perform the bilinear interpolation (2D) in order to get
    // the lat at the source position.
    double lon1=_data.lon(jI,    iI)    *(1.0-iF)  *(1.0-jF);
    double lon2=_data.lon(jI+1,  iI)    *(1.0-iF)  *     jF;
    double lon3=_data.lon(jI+1,  iI+1)  *     iF   *     jF;
    double lon4=_data.lon(jI  ,  iI+1)  *     iF   *(1.0-jF);

    // The current lon (longitude) at the source position
    lon=lon1+lon2+lon3+lon4;


    // Perform the bilinear interpolation (2D) in order to get
    // the lat at the source position.
    double lat1=_data.lat(jI,    iI)    *(1.0-iF)  *(1.0-jF);
    double lat2=_data.lat(jI+1,  iI)    *(1.0-iF)  *     jF;
    double lat3=_data.lat(jI+1,  iI+1)  *     iF   *     jF;
    double lat4=_data.lat(jI  ,  iI+1)  *     iF   *(1.0-jF);

    // The current lat (latitude) at the source position
    lat=lat1+lat2+lat3+lat4;

    // Perform the bilinear interpolation (2D) in order to get
    // the h (depth) at the particle position.
    double h1=_data.h(jI,    iI)    *(1.0-iF)  *(1.0-jF);
    double h2=_data.h(jI+1,  iI)    *(1.0-iF)  *     jF;
    double h3=_data.h(jI+1,  iI+1)  *     iF   *     jF;
    double h4=_data.h(jI  ,  iI+1)  *     iF   *(1.0-jF);

    // The current h (depth) at the particle position
    double h=h1+h2+h3+h4;

    double aDep = abs(
            abs(h * _data.sW(kI-1)) -
            abs(h * _data.sW(kI))
    );
    dep=-(h*abs(_data.sW(kI))+abs(kF*aDep));
}

void OceanModelAdapter::deplatlon2kji(double dep, double lat, double lon, double &k, double &j, double &i) {
    size_t eta_rho = _data.mask.Nx();
    size_t xi_rho = _data.mask.Ny();
    size_t s_w = _data.w.Ny();

    // Reject positions that cannot be placed on the grid
    if (!std::isfinite(lat) || !std::isfinite(lon)) {
        throw std::invalid_argument("Source latitude and longitude must be finite");
    }

    // Invert the bilinear lon/lat interpolation of kji2deplatlon cell by cell, in
    // source order (j, then i), accepting the first cell that contains the point.
    // The bounding box only discards cells that cannot contain it.
    const double fractionTolerance=1e-9;
    bool found=false;
    for (int jC=0; jC+1<(int)eta_rho && !found; jC++) {
        for (int iC=0; iC+1<(int)xi_rho && !found; iC++) {
            double lon00=_data.lon(jC,iC),   lon10=_data.lon(jC+1,iC);
            double lon11=_data.lon(jC+1,iC+1), lon01=_data.lon(jC,iC+1);
            double lat00=_data.lat(jC,iC),   lat10=_data.lat(jC+1,iC);
            double lat11=_data.lat(jC+1,iC+1), lat01=_data.lat(jC,iC+1);

            double lonMin=min(min(lon00,lon10),min(lon11,lon01)), lonMax=max(max(lon00,lon10),max(lon11,lon01));
            double latMin=min(min(lat00,lat10),min(lat11,lat01)), latMax=max(max(lat00,lat10),max(lat11,lat01));
            double lonPad=fractionTolerance*(lonMax-lonMin), latPad=fractionTolerance*(latMax-latMin);
            if (lon<lonMin-lonPad || lon>lonMax+lonPad || lat<latMin-latPad || lat>latMax+latPad) continue;

            // Newton iteration on the bilinear map (iF, jF) -> (lon, lat)
            double iF=0.5, jF=0.5;
            bool converged=false;
            for (int iteration=0; iteration<50; iteration++) {
                double rLon=lon00*(1.0-iF)*(1.0-jF)+lon10*(1.0-iF)*jF+lon11*iF*jF+lon01*iF*(1.0-jF)-lon;
                double rLat=lat00*(1.0-iF)*(1.0-jF)+lat10*(1.0-iF)*jF+lat11*iF*jF+lat01*iF*(1.0-jF)-lat;
                double dLonDi=(lon01-lon00)*(1.0-jF)+(lon11-lon10)*jF;
                double dLonDj=(lon10-lon00)*(1.0-iF)+(lon11-lon01)*iF;
                double dLatDi=(lat01-lat00)*(1.0-jF)+(lat11-lat10)*jF;
                double dLatDj=(lat10-lat00)*(1.0-iF)+(lat11-lat01)*iF;
                double det=dLonDi*dLatDj-dLonDj*dLatDi;
                if (!std::isfinite(det) || det==0) break;
                double stepI=( dLatDj*rLon-dLonDj*rLat)/det;
                double stepJ=(-dLatDi*rLon+dLonDi*rLat)/det;
                iF-=stepI; jF-=stepJ;
                if (!std::isfinite(iF) || !std::isfinite(jF)) break;
                if (abs(stepI)<1e-11 && abs(stepJ)<1e-11) { converged=true; break; }
            }
            if (!converged ||
                iF<-fractionTolerance || iF>1.0+fractionTolerance ||
                jF<-fractionTolerance || jF>1.0+fractionTolerance) continue;

            // Snap fractions within the tolerance onto the cell edges, so that membership of
            // a node line does not depend on round-off
            if (abs(iF)<=fractionTolerance) iF=0.0; else if (abs(iF-1.0)<=fractionTolerance) iF=1.0;
            if (abs(jF)<=fractionTolerance) jF=0.0; else if (abs(jF-1.0)<=fractionTolerance) jF=1.0;

            // The outermost north and east node lines are outside the interpolation domain
            if (!NumericalHelpers::validInterpolationCell(NumericalHelpers::horizontalCell(jC+jF),
                                                         NumericalHelpers::horizontalCell(iC+iF),
                                                         eta_rho,xi_rho)) continue;
            j=jC+jF;
            i=iC+iF;
            found=true;
        }
    }

    if (!found) {
        throw std::out_of_range("Position lat="+std::to_string(lat)+" lon="+std::to_string(lon)+
                                " is outside the ocean model interpolation domain");
    }

    // Get the integer part and the fraction part of particle j
    auto jI=NumericalHelpers::horizontalCell(j); double jF=j-jI;

    // Get the integer part and the fraction part of particle i
    auto iI=NumericalHelpers::horizontalCell(i); double iF=i-iI;

    // Convert dep to positive down
    dep = abs(dep);

    // Perform the bilinear interpolation (2D) in order to get
    // the h (depth) at the particle position.
    double h1=_data.h(jI,    iI)    *(1.0-iF)  *(1.0-jF);
    double h2=_data.h(jI+1,  iI)    *(1.0-iF)  *     jF;
    double h3=_data.h(jI+1,  iI+1)  *     iF   *     jF;
    double h4=_data.h(jI  ,  iI+1)  *     iF   *(1.0-jF);

    // The current h (depth) at the particle position
    double h=h1+h2+h3+h4;

    // Reject depths that cannot be placed on the vertical coordinate
    if (!std::isfinite(dep) || !std::isfinite(h)) {
        throw std::invalid_argument("Source depth and local bathymetry must be finite");
    }

    // The lowest valid particle level, the same limit used by the solver
    int kBottom=-(int) s_w + 2;

    // Invert the kji2deplatlon mapping:
    // dep = h|sW(kI)| + kF (h|sW(kI-1)| - h|sW(kI)|), with k = kI - kF and 0 <= kF < 1
    if (dep <= h * abs(_data.sW(0))) {
        // The position is on the surface
        k=0;
    } else if (dep >= h * abs(_data.sW(kBottom))) {
        // The position is about at the bottom
        k=kBottom;
    } else {
        // Walk downward from the surface until the W interval containing dep is found
        int kI=0;
        while (kI-1 > kBottom && dep >= h * abs(_data.sW(kI-1))) kI--;
        double upper = h * abs(_data.sW(kI));
        double aDep = abs(h * abs(_data.sW(kI-1)) - upper);
        double kF = (dep - upper) / aDep;
        k = kI - kF;
    }
}

// Returns -1 if a < 0 and 1 if a > 0
double OceanModelAdapter::sgn(double a) { return (a > 0) - (a < 0); }
