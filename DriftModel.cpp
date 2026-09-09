#include "DriftModel.hpp"

#include <stdexcept>
#include <string>

namespace {
// Slopes are converted from percent to dimensionless fractions and offsets/stddevs
// from cm s-1 to m s-1. Values are from the USCG/SAROPS OBJECTPROP.DAT lineage,
// as distributed by OpenDrift (updated 2011-08-25).
const DriftObjectDefinition definitions[] = {
        {DriftObjectType::PASSIVE,"PASSIVE",{0,0,0,0,0,0}},
        {DriftObjectType::PERSON_IN_WATER,"PERSON_IN_WATER",driftObjectCoefficients(DriftObjectType::PERSON_IN_WATER)},
        {DriftObjectType::LIFERAFT_NO_DROGUE,"LIFERAFT_NO_DROGUE",driftObjectCoefficients(DriftObjectType::LIFERAFT_NO_DROGUE)},
        {DriftObjectType::LIFERAFT_DROGUE,"LIFERAFT_DROGUE",driftObjectCoefficients(DriftObjectType::LIFERAFT_DROGUE)},
        {DriftObjectType::GENERIC_VESSEL,"GENERIC_VESSEL",driftObjectCoefficients(DriftObjectType::GENERIC_VESSEL)},
        {DriftObjectType::SHIPPING_CONTAINER,"SHIPPING_CONTAINER",driftObjectCoefficients(DriftObjectType::SHIPPING_CONTAINER)}
};
}

const DriftObjectDefinition& DriftObjectCatalog::definition(DriftObjectType type) {
    for (const auto& item:definitions) if (item.type==type) return item;
    throw std::runtime_error("Unknown drift object type id: " + std::to_string(static_cast<int>(type)));
}

DriftObjectType DriftObjectCatalog::type(const char *name) {
    for (const auto& item:definitions) if (std::string(item.name)==name) return item.type;
    throw std::runtime_error("Unknown drift.object_type: " + std::string(name));
}

const char *DriftObjectCatalog::name(DriftObjectType type) { return definition(type).name; }
