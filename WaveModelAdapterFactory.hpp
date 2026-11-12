#ifndef WACOMMPLUSPLUS_WAVEMODELADAPTERFACTORY_HPP
#define WACOMMPLUSPLUS_WAVEMODELADAPTERFACTORY_HPP
#include "WaveModelAdapter.hpp"
#include <memory>
class WaveModelAdapterFactory { public: static std::shared_ptr<WaveModelAdapter> create(const std::string&,std::string&,const std::string& sourceCrs=""); };
#endif
