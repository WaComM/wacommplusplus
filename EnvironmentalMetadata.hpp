#ifndef WACOMMPLUSPLUS_ENVIRONMENTALMETADATA_HPP
#define WACOMMPLUSPLUS_ENVIRONMENTALMETADATA_HPP

#include "Array.h"
#include <netcdf>
#include <string>

namespace EnvironmentalMetadata {
    std::string attribute(netCDF::NcVar& variable,const std::string& name);
    void requireVelocity(netCDF::NcVar& variable,const std::string& label);
    void requireLongitude(netCDF::NcVar& variable,const std::string& label);
    void requireLatitude(netCDF::NcVar& variable,const std::string& label);
    void requireProjectedCoordinate(netCDF::NcVar& variable,const std::string& label);
    void readCfTime(netCDF::NcVar& variable,Array::Array1<double>& destination,const std::string& label);
}

#endif
