#include "DriftModel.hpp"

#include <stdexcept>
#include <string>

namespace {
// Slopes are converted from percent to dimensionless fractions and offsets/stddevs
// from cm s-1 to m s-1. Values are from the USCG/SAROPS OBJECTPROP.DAT lineage,
// as distributed by OpenDrift (updated 2011-08-25).
const DriftObjectDefinition definitions[] = {
        {DriftObjectType::PASSIVE,"PASSIVE","PASSIVE","No wind-relative object motion",driftObjectCoefficients(DriftObjectType::PASSIVE)},
#define OBJECT(type,key,text) {DriftObjectType::type,#type,key,text,driftObjectCoefficients(DriftObjectType::type)}
        OBJECT(PERSON_IN_WATER,"PIW-1","Person in water, unknown state, mean"),
        OBJECT(LIFERAFT_NO_DROGUE,"LIFE-RAFT-NB-4","Life raft, no ballast, canopy, no drogue"),
        OBJECT(LIFERAFT_DROGUE,"LIFE-RAFT-NB-5","Life raft, no ballast, canopy, with drogue"),
        OBJECT(GENERIC_VESSEL,"FISHING-VESSEL-1","Fishing vessel, general mean"),
        OBJECT(SHIPPING_CONTAINER,"CONTAINER-2","20-ft container, 80 percent submerged"),
        OBJECT(PERSON_IN_WATER_PFD,"PIW-2","Person in water, conscious, vertical type-III PFD"),
        OBJECT(PERSON_IN_WATER_SURVIVAL_SUIT,"PIW-4","Person in water, survival suit, face up"),
        OBJECT(PERSON_IN_WATER_DECEASED,"PIW-6","Person in water, deceased, face down"),
        OBJECT(LIFERAFT_DEEP_BALLAST,"LIFE-RAFT-DB-10","Life raft, deep ballast, general mean"),
        OBJECT(LIFERAFT_DEEP_BALLAST_CAPSIZED,"LIFE-RAFT-DB-21","Life raft, deep ballast, capsized"),
        OBJECT(KAYAK_WITH_PERSON,"PERSON-POWERED-VESSEL-1","Sea kayak with person on aft deck"),
        OBJECT(SURFBOARD_WITH_PERSON,"PERSON-POWERED-VESSEL-2","Surfboard with person"),
        OBJECT(SKIFF,"SKIFF-1","Modified-V or cathedral-hull skiff"),
        OBJECT(SKIFF_CAPSIZED,"SKIFF-3","Swamped or capsized skiff"),
        OBJECT(SPORT_BOAT,"SPORT-BOAT","Sport boat without canvas, modified-V hull"),
        OBJECT(COASTAL_FREIGHTER,"COASTAL-FREIGHTER","Coastal freighter"),
        OBJECT(SAILBOAT,"SAILBOAT-1","Monohull sailboat, mean"),
        OBJECT(FISHING_VESSEL_DEBRIS,"FV-DEBRIS","Fishing-vessel debris"),
        OBJECT(OIL_DRUM,"OIL-DRUM","55-gallon (220 litre) oil drum"),
        OBJECT(WWII_MINE,"MINE","World War II L-MK2 mine"),
        OBJECT(REFUGEE_RAFT_NO_SAIL,"REFUGEE-RAFT-1","Cuban refugee raft without sail"),
        OBJECT(MEDICAL_WASTE,"MED-WASTE-1","Medical waste, mean"),
#undef OBJECT
};

const DriftProcessDefinition processes[] = {
        {DriftProcessType::OCEAN_CURRENT,"ocean_current","ocean adapter velocity","m s-1",false,false},
        {DriftProcessType::LEEWAY,"leeway","drift.model=leeway","m s-1",false,false},
        {DriftProcessType::STOKES_DRIFT,"stokes_drift","environment.wave.adapter=WW3","m s-1",false,false},
        {DriftProcessType::HORIZONTAL_DIFFUSION,"horizontal_diffusion","physics.random and physics.sigma","m",true,false},
        {DriftProcessType::VERTICAL_DIFFUSION,"vertical_diffusion","physics.random and ocean AKt","m2 s-1",true,false},
        {DriftProcessType::SETTLING_RISE,"settling_rise","physics.sv","m s-1",false,false},
        {DriftProcessType::DECAY,"decay","physics.survprob and physics.tau0","s",false,false},
        {DriftProcessType::WIND_ERROR,"wind_error","environment.wind.uncertainty_stddev","m s-1",true,false},
        {DriftProcessType::JIBING,"jibing","drift.jibe_probability_per_hour","h-1",true,true}
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

const DriftProcessDefinition& DriftProcessCatalog::definition(DriftProcessType type) {
    for (const auto& item:processes) if (item.type==type) return item;
    throw std::runtime_error("Unknown drift process type id: " + std::to_string(static_cast<int>(type)));
}

DriftProcessType DriftProcessCatalog::type(const char *name) {
    for (const auto& item:processes) if (std::string(item.name)==name) return item.type;
    throw std::runtime_error("Unknown drift process: " + std::string(name));
}

const char *DriftProcessCatalog::name(DriftProcessType type) { return definition(type).name; }
