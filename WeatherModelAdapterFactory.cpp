#include "WeatherModelAdapterFactory.hpp"
#include "WeatherModelAdapters/WRFAdapter.hpp"
#include <stdexcept>
std::shared_ptr<WeatherModelAdapter> WeatherModelAdapterFactory::create(const std::string& model,std::string& file) {
    if (model=="WRF") return std::make_shared<WRFAdapter>(file);
    throw std::runtime_error("Unknown or unavailable weather model adapter: " + model);
}
