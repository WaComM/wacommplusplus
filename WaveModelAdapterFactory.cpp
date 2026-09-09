#include "WaveModelAdapterFactory.hpp"
#include "WaveModelAdapters/WW3Adapter.hpp"
#include <stdexcept>
std::shared_ptr<WaveModelAdapter> WaveModelAdapterFactory::create(const std::string& model,std::string& file) {
    if (model=="WW3") return std::make_shared<WW3Adapter>(file);
    throw std::runtime_error("Unknown or unavailable wave model adapter: " + model);
}
