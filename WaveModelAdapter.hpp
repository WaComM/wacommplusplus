#ifndef WACOMMPLUSPLUS_WAVEMODELADAPTER_HPP
#define WACOMMPLUSPLUS_WAVEMODELADAPTER_HPP

#include "Array.h"
#include <netcdf>

using namespace Array;
using namespace netCDF;

class WaveModelAdapter {
public:
    virtual ~WaveModelAdapter() = default;
    virtual void process()=0;
    void appendBoundaryRecord(WaveModelAdapter &adapter, int record, bool prepend);
    Array1<double>& Time();
    Array2<double>& Lon();
    Array2<double>& Lat();
    Array3<float>& StokesU();
    Array3<float>& StokesV();
private:
    Array1<double> time;
    Array2<double> lon,lat;
    Array3<float> stokesU,stokesV;
};

#endif
