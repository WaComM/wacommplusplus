#include "WaveModelAdapterFactory.hpp"
#include "WaveModelAdapters/WW3Adapter.hpp"
#include <stdexcept>
std::shared_ptr<WaveModelAdapter> WaveModelAdapterFactory::create(const std::string& model,std::string& file,const std::string& sourceCrs) {
    if (model=="WW3") return std::make_shared<WW3Adapter>(file,sourceCrs);
    throw std::runtime_error("Unknown or unavailable wave model adapter: " + model);
}
