#include <netcdf>

#include <cmath>
#include <stdexcept>

using namespace netCDF;

int main() {
    NcFile remote("http://test.opendap.org/opendap/data/nc/coads_climatology.nc",NcFile::read);
    NcVar longitude=remote.getVar("COADSX");
    NcVar latitude=remote.getVar("COADSY");
    NcVar temperature=remote.getVar("SST");
    if (longitude.isNull() || latitude.isNull() || temperature.isNull())
        throw std::runtime_error("The remote OPeNDAP fixture is missing expected variables");
    if (temperature.getDimCount()!=3 || temperature.getDim(0).getSize()!=12 ||
        temperature.getDim(1).getSize()!=90 || temperature.getDim(2).getSize()!=180)
        throw std::runtime_error("The remote OPeNDAP fixture has unexpected dimensions");

    double lon=0,lat=0;
    float values[4]={};
    longitude.getVar({0},{1},&lon);
    latitude.getVar({0},{1},&lat);
    temperature.getVar({0,0,0},{1,2,2},values);
    if (!std::isfinite(lon) || !std::isfinite(lat))
        throw std::runtime_error("Remote coordinate hyperslab contains a non-finite value");
    bool finite=false;
    for (float value:values) finite=finite || std::isfinite(value);
    if (!finite) throw std::runtime_error("Remote data hyperslab contains no finite values");
}
