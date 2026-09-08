#include "OceanModelAdapterFactory.hpp"

#include "OceanModelAdapters/HYCOMAdapter.hpp"
#include "OceanModelAdapters/NEMOAdapter.hpp"
#include "OceanModelAdapters/ROMSAdapter.hpp"
#include "OceanModelAdapters/WacommAdapter.hpp"

#include <stdexcept>

std::shared_ptr<OceanModelAdapter> OceanModelAdapterFactory::create(const std::string &model, std::string &fileName) {
    if (model == "ROMS") return std::make_shared<ROMSAdapter>(fileName);
    if (model == "NEMO") return std::make_shared<NEMOAdapter>(fileName);
    if (model == "HYCOM") return std::make_shared<HYCOMAdapter>(fileName);
    if (model == "WACOMM") return std::make_shared<WacommAdapter>(fileName);
    throw std::runtime_error("Unknown or unavailable ocean model adapter: " + model);
}
