#ifndef WACOMMPLUSPLUS_WEATHERMODELADAPTERFACTORY_HPP
#define WACOMMPLUSPLUS_WEATHERMODELADAPTERFACTORY_HPP
#include "WeatherModelAdapter.hpp"
#include <memory>
class WeatherModelAdapterFactory { public: static std::shared_ptr<WeatherModelAdapter> create(const std::string&,std::string&,const std::string& sourceCrs=""); };
#endif
