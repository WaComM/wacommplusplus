#include "EnvironmentalRegridder.hpp"
#include <cmath>
#include <stdexcept>
#include <vector>

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
    for (int t=0;t<source.Nx();t++) for (int j=0;j<eta;j++) for (int i=0;i<xi;i++)
        if (!std::isfinite(source(t,j,i))) throw std::runtime_error("Environmental source field must be finite before regridding");
    Array::Array3<float> result(source.Nx(),targetLon.Nx(),targetLon.Ny());
    for (int j=0;j<targetLon.Nx();j++) for (int i=0;i<targetLon.Ny();i++) {
        if (!std::isfinite(targetLon(j,i)) || !std::isfinite(targetLat(j,i)))
            throw std::runtime_error("Environmental target coordinates must be finite");
        Bracket x=bracket(longitude,targetLon(j,i),"longitude");
        Bracket y=bracket(latitude,targetLat(j,i),"latitude");
        for (int t=0;t<source.Nx();t++) {
            double lower=source(t,y.lower,x.lower)*(1-x.weight)+source(t,y.lower,x.lower+1)*x.weight;
            double upper=source(t,y.lower+1,x.lower)*(1-x.weight)+source(t,y.lower+1,x.lower+1)*x.weight;
            result(t,j,i)=static_cast<float>(lower*(1-y.weight)+upper*y.weight);
        }
    }
    return result;
}
