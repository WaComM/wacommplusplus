#include "WeatherModelAdapterFactory.hpp"
#include "WeatherModelAdapters/WRFAdapter.hpp"
#include <stdexcept>
std::shared_ptr<WeatherModelAdapter> WeatherModelAdapterFactory::create(const std::string& model,std::string& file,const std::string& sourceCrs) {
    if (model=="WRF") return std::make_shared<WRFAdapter>(file,sourceCrs);
    throw std::runtime_error("Unknown or unavailable weather model adapter: " + model);
}
