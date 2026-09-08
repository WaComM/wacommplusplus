#ifndef WACOMMPLUSPLUS_OCEANMODELADAPTERFACTORY_HPP
#define WACOMMPLUSPLUS_OCEANMODELADAPTERFACTORY_HPP

#include "OceanModelAdapter.hpp"

#include <memory>
#include <string>

class OceanModelAdapterFactory {
public:
    static std::shared_ptr<OceanModelAdapter> create(const std::string &model, std::string &fileName);
};

#endif
