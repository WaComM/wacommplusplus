#ifndef WACOMMPLUSPLUS_WEATHERMODELADAPTER_HPP
#define WACOMMPLUSPLUS_WEATHERMODELADAPTER_HPP

#include "Array.h"
#include <netcdf>
#include <string>

using namespace Array;
using namespace netCDF;

struct weathermodel_data {
    Array1<double> time;
    Array2<double> lon;
    Array2<double> lat;
    Array3<float> windU10;
    Array3<float> windV10;
};

class WeatherModelAdapter {
public:
    virtual ~WeatherModelAdapter() = default;
    virtual void process()=0;
    void appendBoundaryRecord(WeatherModelAdapter &adapter, int record, bool prepend);
    Array1<double>& Time();
    Array2<double>& Lon();
    Array2<double>& Lat();
    Array3<float>& WindU10();
    Array3<float>& WindV10();
private:
    weathermodel_data _data;
};

#endif
