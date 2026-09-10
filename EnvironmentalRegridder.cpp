#include "EnvironmentalRegridder.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>
#ifdef WACOMM_USE_PROJ
#include <proj.h>
#endif

namespace {
struct Bracket { int lower; double weight; };

Bracket bracket(const std::vector<double>& axis,double value,const char *name) {
    bool increasing=axis.back()>axis.front();
    for (size_t i=1;i<axis.size();i++)
        if ((increasing && axis[i]<=axis[i-1]) || (!increasing && axis[i]>=axis[i-1]))
            throw std::runtime_error(std::string("Environmental ") + name + " axis must be strictly monotonic");
    double minimum=increasing ? axis.front() : axis.back(),maximum=increasing ? axis.back() : axis.front();
    if (value<minimum-1.e-10 || value>maximum+1.e-10)
        throw std::runtime_error(std::string("Environmental target lies outside the ") + name + " axis; extrapolation is prohibited");
    for (size_t i=0;i+1<axis.size();i++) {
        if ((increasing && value>=axis[i]-1.e-10 && value<=axis[i+1]+1.e-10) ||
            (!increasing && value<=axis[i]+1.e-10 && value>=axis[i+1]-1.e-10))
            return {(int)i,(value-axis[i])/(axis[i+1]-axis[i])};
    }
    throw std::runtime_error(std::string("Unable to bracket environmental ") + name + " coordinate");
}

void unwrapLongitude(std::vector<double>& longitude) {
    for (size_t i=1;i<longitude.size();i++) {
        while (longitude[i]-longitude[i-1]>180) longitude[i]-=360;
        while (longitude[i]-longitude[i-1]<-180) longitude[i]+=360;
    }
}

double longitudeBranch(const std::vector<double>& longitude,double value) {
    double center=.5*(longitude.front()+longitude.back());
    while (value-center>180) value-=360;
    while (value-center<-180) value+=360;
    return value;
}

double nearLongitude(double value,double reference) {
    while (value-reference>180) value-=360;
    while (value-reference<-180) value+=360;
    return value;
}

double bilinear(double f00,double f10,double f01,double f11,double x,double y) {
    return (1-x)*(1-y)*f00+x*(1-y)*f10+(1-x)*y*f01+x*y*f11;
}

struct CurvilinearCell {
    int j,i;
    double minimumX,maximumX,minimumY,maximumY;
};

class CurvilinearCellIndex {
public:
    CurvilinearCellIndex(const Array::Array2<double>& longitude,const Array::Array2<double>& latitude,bool cyclicLongitude=true): cyclicLongitude(cyclicLongitude) {
        double reference=longitude(0,0);
        if (!std::isfinite(reference))
            throw std::runtime_error("Environmental curvilinear source coordinates must be finite");
        for (int j=0;j+1<longitude.Nx();j++) for (int i=0;i+1<longitude.Ny();i++) {
            double x00=longitude(j,i),x10=cyclicLongitude ? nearLongitude(longitude(j,i+1),x00) : longitude(j,i+1);
            double x01=cyclicLongitude ? nearLongitude(longitude(j+1,i),x00) : longitude(j+1,i);
            double x11=cyclicLongitude ? nearLongitude(longitude(j+1,i+1),x00) : longitude(j+1,i+1);
            double y00=latitude(j,i),y10=latitude(j,i+1),y01=latitude(j+1,i),y11=latitude(j+1,i+1);
            if (!std::isfinite(x00) || !std::isfinite(x10) || !std::isfinite(x01) || !std::isfinite(x11) ||
                !std::isfinite(y00) || !std::isfinite(y10) || !std::isfinite(y01) || !std::isfinite(y11))
                throw std::runtime_error("Environmental curvilinear source coordinates must be finite");
            double center=.25*(x00+x10+x01+x11);
            double shift=cyclicLongitude ? 360*std::round((reference-center)/360) : 0;
            CurvilinearCell cell={j,i,std::min(std::min(x00,x10),std::min(x01,x11))+shift,
                    std::max(std::max(x00,x10),std::max(x01,x11))+shift,
                    std::min(std::min(y00,y10),std::min(y01,y11)),
                    std::max(std::max(y00,y10),std::max(y01,y11))};
            cells.push_back(cell);
        }
        minimumX=cells.front().minimumX; maximumX=cells.front().maximumX;
        minimumY=cells.front().minimumY; maximumY=cells.front().maximumY;
        for (const auto& cell:cells) {
            minimumX=std::min(minimumX,cell.minimumX); maximumX=std::max(maximumX,cell.maximumX);
            minimumY=std::min(minimumY,cell.minimumY); maximumY=std::max(maximumY,cell.maximumY);
        }
        double width=std::max(maximumX-minimumX,1.e-12),height=std::max(maximumY-minimumY,1.e-12);
        double scale=std::sqrt(cells.size());
        binsX=std::max(1,(int)std::round(scale*std::sqrt(width/height)));
        binsY=std::max(1,(int)std::round(scale*std::sqrt(height/width)));
        binsX=std::min(binsX,(int)cells.size()); binsY=std::min(binsY,(int)cells.size());
        bins.resize((size_t)binsX*binsY);
        for (size_t index=0;index<cells.size();index++) {
            int i0=xBin(cells[index].minimumX),i1=xBin(cells[index].maximumX);
            int j0=yBin(cells[index].minimumY),j1=yBin(cells[index].maximumY);
            for (int j=j0;j<=j1;j++) for (int i=i0;i<=i1;i++) bins[(size_t)j*binsX+i].push_back(index);
        }
        longitudeReference=reference;
    }

    const std::vector<size_t>& candidates(double& longitude,double latitude) const {
        if (cyclicLongitude) longitude=nearLongitude(longitude,longitudeReference);
        if (longitude<minimumX-1.e-10 || longitude>maximumX+1.e-10 ||
            latitude<minimumY-1.e-10 || latitude>maximumY+1.e-10) return empty;
        return bins[(size_t)yBin(latitude)*binsX+xBin(longitude)];
    }

    const CurvilinearCell& cell(size_t index) const { return cells[index]; }

private:
    int xBin(double value) const {
        if (maximumX==minimumX) return 0;
        return std::max(0,std::min(binsX-1,(int)((value-minimumX)/(maximumX-minimumX)*binsX)));
    }
    int yBin(double value) const {
        if (maximumY==minimumY) return 0;
        return std::max(0,std::min(binsY-1,(int)((value-minimumY)/(maximumY-minimumY)*binsY)));
    }
    std::vector<CurvilinearCell> cells;
    std::vector<std::vector<size_t>> bins;
    std::vector<size_t> empty;
    double minimumX=0,maximumX=0,minimumY=0,maximumY=0,longitudeReference=0;
    int binsX=1,binsY=1;
    bool cyclicLongitude=true;
};
}

Array::Array3<float> EnvironmentalRegridder::bilinearGeographic(
        const Array::Array2<double>& sourceLon,const Array::Array2<double>& sourceLat,
        const Array::Array3<float>& source,const Array::Array2<double>& targetLon,
        const Array::Array2<double>& targetLat) {
    size_t eta=sourceLon.Nx(),xi=sourceLon.Ny();
    if (eta<2 || xi<2 || sourceLat.Nx()!=eta || sourceLat.Ny()!=xi ||
        source.Ny()!=eta || source.Nz()!=xi)
        throw std::runtime_error("Environmental bilinear regridding requires compatible source dimensions of at least 2x2");
    if (targetLon.Nx()!=targetLat.Nx() || targetLon.Ny()!=targetLat.Ny())
        throw std::runtime_error("Environmental target longitude and latitude dimensions differ");
    std::vector<double> longitude(xi),latitude(eta);
    for (int i=0;i<xi;i++) longitude[i]=sourceLon(0,i);
    for (int j=0;j<eta;j++) latitude[j]=sourceLat(j,0);
    for (int j=0;j<eta;j++) for (int i=0;i<xi;i++)
        if (!std::isfinite(sourceLon(j,i)) || !std::isfinite(sourceLat(j,i)) ||
            std::abs(sourceLon(j,i)-longitude[i])>1.e-10 || std::abs(sourceLat(j,i)-latitude[j])>1.e-10)
            throw std::runtime_error("Environmental bilinear regridding currently requires a rectilinear geographic source grid");
    unwrapLongitude(longitude);
    if (std::abs(longitude.back()-longitude.front())>360+1.e-10)
        throw std::runtime_error("Environmental longitude axis spans more than one revolution");
    for (int t=0;t<source.Nx();t++) for (int j=0;j<eta;j++) for (int i=0;i<xi;i++)
        if (!std::isfinite(source(t,j,i))) throw std::runtime_error("Environmental source field must be finite before regridding");
    Array::Array3<float> result(source.Nx(),targetLon.Nx(),targetLon.Ny());
    for (int j=0;j<targetLon.Nx();j++) for (int i=0;i<targetLon.Ny();i++) {
        if (!std::isfinite(targetLon(j,i)) || !std::isfinite(targetLat(j,i)))
            throw std::runtime_error("Environmental target coordinates must be finite");
        Bracket x=bracket(longitude,longitudeBranch(longitude,targetLon(j,i)),"longitude");
        Bracket y=bracket(latitude,targetLat(j,i),"latitude");
        for (int t=0;t<source.Nx();t++) {
            double lower=source(t,y.lower,x.lower)*(1-x.weight)+source(t,y.lower,x.lower+1)*x.weight;
            double upper=source(t,y.lower+1,x.lower)*(1-x.weight)+source(t,y.lower+1,x.lower+1)*x.weight;
            result(t,j,i)=static_cast<float>(lower*(1-y.weight)+upper*y.weight);
        }
    }
    return result;
}

Array::Array3<float> EnvironmentalRegridder::bilinearCurvilinearGeographic(
        const Array::Array2<double>& sourceLon,const Array::Array2<double>& sourceLat,
        const Array::Array3<float>& source,const Array::Array2<double>& targetLon,
        const Array::Array2<double>& targetLat) {
    size_t eta=sourceLon.Nx(),xi=sourceLon.Ny();
    if (eta<2 || xi<2 || sourceLat.Nx()!=eta || sourceLat.Ny()!=xi ||
        source.Ny()!=eta || source.Nz()!=xi)
        throw std::runtime_error("Environmental curvilinear regridding requires compatible source dimensions of at least 2x2");
    if (targetLon.Nx()!=targetLat.Nx() || targetLon.Ny()!=targetLat.Ny())
        throw std::runtime_error("Environmental target longitude and latitude dimensions differ");
    for (int t=0;t<source.Nx();t++) for (int j=0;j<eta;j++) for (int i=0;i<xi;i++)
        if (!std::isfinite(source(t,j,i))) throw std::runtime_error("Environmental source field must be finite before regridding");
    CurvilinearCellIndex cellIndex(sourceLon,sourceLat);
    Array::Array3<float> result(source.Nx(),targetLon.Nx(),targetLon.Ny());
    for (int tj=0;tj<targetLon.Nx();tj++) for (int ti=0;ti<targetLon.Ny();ti++) {
        double targetX=targetLon(tj,ti),targetY=targetLat(tj,ti),foundX=0,foundY=0;
        if (!std::isfinite(targetX) || !std::isfinite(targetY))
            throw std::runtime_error("Environmental target coordinates must be finite");
        int foundJ=-1,foundI=-1;
        const auto& candidates=cellIndex.candidates(targetX,targetY);
        for (size_t candidate:candidates) {
            const auto& indexedCell=cellIndex.cell(candidate);
            int j=indexedCell.j,i=indexedCell.i;
            double x00=nearLongitude(sourceLon(j,i),targetX),x10=nearLongitude(sourceLon(j,i+1),targetX);
            double x01=nearLongitude(sourceLon(j+1,i),targetX),x11=nearLongitude(sourceLon(j+1,i+1),targetX);
            double y00=sourceLat(j,i),y10=sourceLat(j,i+1),y01=sourceLat(j+1,i),y11=sourceLat(j+1,i+1);
            if (!std::isfinite(x00) || !std::isfinite(x10) || !std::isfinite(x01) || !std::isfinite(x11) ||
                !std::isfinite(y00) || !std::isfinite(y10) || !std::isfinite(y01) || !std::isfinite(y11))
                throw std::runtime_error("Environmental curvilinear source coordinates must be finite");
            if (targetX<indexedCell.minimumX-1.e-10 || targetX>indexedCell.maximumX+1.e-10 ||
                targetY<indexedCell.minimumY-1.e-10 || targetY>indexedCell.maximumY+1.e-10) continue;
            double x=.5,y=.5;
            for (int iteration=0;iteration<20;iteration++) {
                double mappedX=bilinear(x00,x10,x01,x11,x,y),mappedY=bilinear(y00,y10,y01,y11,x,y);
                double dXdx=(1-y)*(x10-x00)+y*(x11-x01),dXdy=(1-x)*(x01-x00)+x*(x11-x10);
                double dYdx=(1-y)*(y10-y00)+y*(y11-y01),dYdy=(1-x)*(y01-y00)+x*(y11-y10);
                double determinant=dXdx*dYdy-dXdy*dYdx;
                if (std::abs(determinant)<1.e-14) break;
                double residualX=mappedX-targetX,residualY=mappedY-targetY;
                x-=(dYdy*residualX-dXdy*residualY)/determinant;
                y-=(-dYdx*residualX+dXdx*residualY)/determinant;
            }
            double residual=std::hypot(bilinear(x00,x10,x01,x11,x,y)-targetX,
                                       bilinear(y00,y10,y01,y11,x,y)-targetY);
            if (x>=-1.e-9 && x<=1+1.e-9 && y>=-1.e-9 && y<=1+1.e-9 && residual<1.e-8) {
                double d00=(x10-x00)*(y01-y00)-(x01-x00)*(y10-y00);
                double d10=(x10-x00)*(y11-y10)-(x11-x10)*(y10-y00);
                double d01=(x11-x01)*(y01-y00)-(x01-x00)*(y11-y01);
                double d11=(x11-x01)*(y11-y10)-(x11-x10)*(y11-y01);
                if (std::abs(d00)<1.e-14 || d00*d10<=0 || d00*d01<=0 || d00*d11<=0)
                    throw std::runtime_error("Environmental curvilinear source contains a folded or singular cell");
                foundJ=j; foundI=i; foundX=std::max(0.0,std::min(1.0,x)); foundY=std::max(0.0,std::min(1.0,y)); break;
            }
        }
        if (foundJ<0) throw std::runtime_error("Environmental target lies outside the curvilinear source grid; extrapolation is prohibited");
        for (int t=0;t<source.Nx();t++)
            result(t,tj,ti)=static_cast<float>(bilinear(source(t,foundJ,foundI),source(t,foundJ,foundI+1),
                                                        source(t,foundJ+1,foundI),source(t,foundJ+1,foundI+1),foundX,foundY));
    }
    return result;
}

Array::Array3<float> EnvironmentalRegridder::bilinearCurvilinearCartesian(
        const Array::Array2<double>& sourceX,const Array::Array2<double>& sourceY,
        const Array::Array3<float>& source,const Array::Array2<double>& targetX,
        const Array::Array2<double>& targetY) {
    size_t eta=sourceX.Nx(),xi=sourceX.Ny();
    if (eta<2 || xi<2 || sourceY.Nx()!=eta || sourceY.Ny()!=xi || source.Ny()!=eta || source.Nz()!=xi)
        throw std::runtime_error("Environmental Cartesian curvilinear regridding requires compatible source dimensions of at least 2x2");
    if (targetX.Nx()!=targetY.Nx() || targetX.Ny()!=targetY.Ny())
        throw std::runtime_error("Environmental Cartesian target dimensions differ");
    for (int t=0;t<source.Nx();t++) for (int j=0;j<eta;j++) for (int i=0;i<xi;i++)
        if (!std::isfinite(source(t,j,i))) throw std::runtime_error("Environmental source field must be finite before regridding");
    CurvilinearCellIndex cellIndex(sourceX,sourceY,false);
    Array::Array3<float> result(source.Nx(),targetX.Nx(),targetX.Ny());
    for (int tj=0;tj<targetX.Nx();tj++) for (int ti=0;ti<targetX.Ny();ti++) {
        double targetCoordinateX=targetX(tj,ti),targetCoordinateY=targetY(tj,ti),foundX=0,foundY=0;
        if (!std::isfinite(targetCoordinateX) || !std::isfinite(targetCoordinateY))
            throw std::runtime_error("Environmental target coordinates must be finite");
        int foundJ=-1,foundI=-1;
        const auto& candidates=cellIndex.candidates(targetCoordinateX,targetCoordinateY);
        for (size_t candidate:candidates) {
            const auto& indexedCell=cellIndex.cell(candidate); int j=indexedCell.j,i=indexedCell.i;
            double x00=sourceX(j,i),x10=sourceX(j,i+1),x01=sourceX(j+1,i),x11=sourceX(j+1,i+1);
            double y00=sourceY(j,i),y10=sourceY(j,i+1),y01=sourceY(j+1,i),y11=sourceY(j+1,i+1);
            if (targetCoordinateX<indexedCell.minimumX-1.e-10 || targetCoordinateX>indexedCell.maximumX+1.e-10 ||
                targetCoordinateY<indexedCell.minimumY-1.e-10 || targetCoordinateY>indexedCell.maximumY+1.e-10) continue;
            double x=.5,y=.5;
            for (int iteration=0;iteration<20;iteration++) {
                double mappedX=bilinear(x00,x10,x01,x11,x,y),mappedY=bilinear(y00,y10,y01,y11,x,y);
                double dXdx=(1-y)*(x10-x00)+y*(x11-x01),dXdy=(1-x)*(x01-x00)+x*(x11-x10);
                double dYdx=(1-y)*(y10-y00)+y*(y11-y01),dYdy=(1-x)*(y01-y00)+x*(y11-y10);
                double determinant=dXdx*dYdy-dXdy*dYdx;
                if (std::abs(determinant)<1.e-14) break;
                double residualX=mappedX-targetCoordinateX,residualY=mappedY-targetCoordinateY;
                x-=(dYdy*residualX-dXdy*residualY)/determinant;
                y-=(-dYdx*residualX+dXdx*residualY)/determinant;
            }
            double residual=std::hypot(bilinear(x00,x10,x01,x11,x,y)-targetCoordinateX,
                                       bilinear(y00,y10,y01,y11,x,y)-targetCoordinateY);
            if (x>=-1.e-9 && x<=1+1.e-9 && y>=-1.e-9 && y<=1+1.e-9 && residual<1.e-8) {
                double d00=(x10-x00)*(y01-y00)-(x01-x00)*(y10-y00);
                double d10=(x10-x00)*(y11-y10)-(x11-x10)*(y10-y00);
                double d01=(x11-x01)*(y01-y00)-(x01-x00)*(y11-y01);
                double d11=(x11-x01)*(y11-y10)-(x11-x10)*(y11-y01);
                if (std::abs(d00)<1.e-14 || d00*d10<=0 || d00*d01<=0 || d00*d11<=0)
                    throw std::runtime_error("Environmental Cartesian source contains a folded or singular cell");
                foundJ=j; foundI=i; foundX=std::max(0.0,std::min(1.0,x)); foundY=std::max(0.0,std::min(1.0,y)); break;
            }
        }
        if (foundJ<0) throw std::runtime_error("Environmental target lies outside the Cartesian source grid; extrapolation is prohibited");
        for (int t=0;t<source.Nx();t++)
            result(t,tj,ti)=static_cast<float>(bilinear(source(t,foundJ,foundI),source(t,foundJ,foundI+1),
                                                        source(t,foundJ+1,foundI),source(t,foundJ+1,foundI+1),foundX,foundY));
    }
    return result;
}

Array::Array3<float> EnvironmentalRegridder::bilinearProjected(
        const Array::Array2<double>& sourceX,const Array::Array2<double>& sourceY,
        const Array::Array3<float>& source,const Array::Array2<double>& targetLon,
        const Array::Array2<double>& targetLat,const std::string& sourceCrs) {
#ifndef WACOMM_USE_PROJ
    (void)sourceX; (void)sourceY; (void)source; (void)targetLon; (void)targetLat; (void)sourceCrs;
    throw std::runtime_error("Projected environmental regridding requires a build configured with USE_PROJ=ON");
#else
    if (sourceCrs.empty()) throw std::runtime_error("Projected environmental regridding requires an explicit source_crs");
    size_t eta=sourceX.Nx(),xi=sourceX.Ny();
    if (eta<2 || xi<2 || sourceY.Nx()!=eta || sourceY.Ny()!=xi || source.Ny()!=eta || source.Nz()!=xi)
        throw std::runtime_error("Environmental projected regridding requires compatible source dimensions of at least 2x2");
    if (targetLon.Nx()!=targetLat.Nx() || targetLon.Ny()!=targetLat.Ny())
        throw std::runtime_error("Environmental target longitude and latitude dimensions differ");
    std::vector<double> xAxis(xi),yAxis(eta);
    for (int i=0;i<xi;i++) xAxis[i]=sourceX(0,i);
    for (int j=0;j<eta;j++) yAxis[j]=sourceY(j,0);
    bool rectilinear=true;
    for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
        if (!std::isfinite(sourceX(j,i)) || !std::isfinite(sourceY(j,i)))
            throw std::runtime_error("Environmental projected source coordinates must be finite");
        if (std::abs(sourceX(j,i)-xAxis[i])>1.e-8 || std::abs(sourceY(j,i)-yAxis[j])>1.e-8) rectilinear=false;
    }
    for (int t=0;t<source.Nx();t++) for (int j=0;j<eta;j++) for (int i=0;i<xi;i++)
        if (!std::isfinite(source(t,j,i))) throw std::runtime_error("Environmental source field must be finite before regridding");
    PJ_CONTEXT *context=proj_context_create();
    PJ *raw=context ? proj_create_crs_to_crs(context,"EPSG:4326",sourceCrs.c_str(),nullptr) : nullptr;
    PJ *transform=raw ? proj_normalize_for_visualization(context,raw) : nullptr;
    if (raw) proj_destroy(raw);
    if (!context || !transform) {
        if (transform) proj_destroy(transform);
        if (context) proj_context_destroy(context);
        throw std::runtime_error("Unable to create the declared environmental CRS transformation: " + sourceCrs);
    }
    if (!rectilinear) {
        Array::Array2<double> projectedX(targetLon.Nx(),targetLon.Ny()),projectedY(targetLon.Nx(),targetLon.Ny());
        try {
            for (int j=0;j<targetLon.Nx();j++) for (int i=0;i<targetLon.Ny();i++) {
                if (!std::isfinite(targetLon(j,i)) || !std::isfinite(targetLat(j,i)))
                    throw std::runtime_error("Environmental target coordinates must be finite");
                PJ_COORD projected=proj_trans(transform,PJ_FWD,proj_coord(targetLon(j,i),targetLat(j,i),0,0));
                if (!std::isfinite(projected.xy.x) || !std::isfinite(projected.xy.y))
                    throw std::runtime_error("Environmental CRS transformation produced a non-finite coordinate");
                projectedX(j,i)=projected.xy.x; projectedY(j,i)=projected.xy.y;
            }
            Array::Array3<float> result=bilinearCurvilinearCartesian(sourceX,sourceY,source,projectedX,projectedY);
            proj_destroy(transform); proj_context_destroy(context); return result;
        } catch (...) {
            proj_destroy(transform); proj_context_destroy(context); throw;
        }
    }
    Array::Array3<float> result(source.Nx(),targetLon.Nx(),targetLon.Ny());
    try {
        for (int j=0;j<targetLon.Nx();j++) for (int i=0;i<targetLon.Ny();i++) {
            double longitude=targetLon(j,i),latitude=targetLat(j,i);
            if (!std::isfinite(longitude) || !std::isfinite(latitude))
                throw std::runtime_error("Environmental target coordinates must be finite");
            PJ_COORD projected=proj_trans(transform,PJ_FWD,proj_coord(longitude,latitude,0,0));
            if (!std::isfinite(projected.xy.x) || !std::isfinite(projected.xy.y))
                throw std::runtime_error("Environmental CRS transformation produced a non-finite coordinate");
            Bracket x=bracket(xAxis,projected.xy.x,"projected x");
            Bracket y=bracket(yAxis,projected.xy.y,"projected y");
            for (int t=0;t<source.Nx();t++) {
                double lower=source(t,y.lower,x.lower)*(1-x.weight)+source(t,y.lower,x.lower+1)*x.weight;
                double upper=source(t,y.lower+1,x.lower)*(1-x.weight)+source(t,y.lower+1,x.lower+1)*x.weight;
                result(t,j,i)=static_cast<float>(lower*(1-y.weight)+upper*y.weight);
            }
        }
    } catch (...) {
        proj_destroy(transform); proj_context_destroy(context); throw;
    }
    proj_destroy(transform); proj_context_destroy(context);
    return result;
#endif
}
