#ifndef WACOMMPLUSPLUS_DRIFTMODEL_HPP
#define WACOMMPLUSPLUS_DRIFTMODEL_HPP

#include <cstdint>
#include <cmath>
#include "NumericalHelpers.hpp"

#ifdef __CUDACC__
#define WACOMM_HOST_DEVICE __host__ __device__
#else
#define WACOMM_HOST_DEVICE
#endif

enum class DriftObjectType : std::uint16_t {
    PASSIVE = 0,
    PERSON_IN_WATER = 1,
    LIFERAFT_NO_DROGUE = 2,
    LIFERAFT_DROGUE = 3,
    GENERIC_VESSEL = 4,
    SHIPPING_CONTAINER = 5,
    PERSON_IN_WATER_PFD = 6,
    PERSON_IN_WATER_SURVIVAL_SUIT = 7,
    PERSON_IN_WATER_DECEASED = 8,
    LIFERAFT_DEEP_BALLAST = 9,
    LIFERAFT_DEEP_BALLAST_CAPSIZED = 10,
    KAYAK_WITH_PERSON = 11,
    SURFBOARD_WITH_PERSON = 12,
    SKIFF = 13,
    SKIFF_CAPSIZED = 14,
    SPORT_BOAT = 15,
    COASTAL_FREIGHTER = 16,
    SAILBOAT = 17,
    FISHING_VESSEL_DEBRIS = 18,
    OIL_DRUM = 19,
    WWII_MINE = 20,
    REFUGEE_RAFT_NO_SAIL = 21,
    MEDICAL_WASTE = 22
};

enum class DriftSide : std::int8_t {
    LEFT = -1,
    UNDEFINED = 0,
    RIGHT = 1
};

struct LeewayCoefficients {
    double downwindSlope;
    double downwindIntercept;
    double downwindStdDev;
    double crosswindSlope;
    double crosswindIntercept;
    double crosswindStdDev;
    double leftCrosswindSlope;
    double leftCrosswindIntercept;
    double leftCrosswindStdDev;
};

struct DriftObjectDefinition {
    DriftObjectType type;
    const char *name;
    const char *sourceKey;
    const char *description;
    LeewayCoefficients leeway;
};

enum class DriftProcessType : std::uint8_t {
    OCEAN_CURRENT,
    LEEWAY,
    STOKES_DRIFT,
    HORIZONTAL_DIFFUSION,
    VERTICAL_DIFFUSION,
    SETTLING_RISE,
    DECAY,
    WIND_ERROR,
    JIBING
};

struct DriftProcessDefinition {
    DriftProcessType type;
    const char *name;
    const char *configuration;
    const char *units;
    bool stochastic;
    bool stateful;
};

struct DriftVelocity {
    double u;
    double v;
    double w;
};

static constexpr std::int64_t LEEWAY_ENSEMBLE_RANDOM_INTERVAL=(-9223372036854775807LL-1);
static constexpr std::int64_t LEEWAY_SIDE_RANDOM_INTERVAL=LEEWAY_ENSEMBLE_RANDOM_INTERVAL+1;
static constexpr std::uint64_t LEEWAY_JIBE_RANDOM_COMPONENT=0x4a494245ULL;
static constexpr std::uint64_t LEEWAY_WIND_ERROR_U_COMPONENT=0x57494e4455ULL;
static constexpr std::uint64_t LEEWAY_WIND_ERROR_V_COMPONENT=0x57494e4456ULL;

WACOMM_HOST_DEVICE inline double correlatedNormal(double firstNormal,double independentNormal,
                                                   double correlation) {
    return correlation*firstNormal+std::sqrt(1-correlation*correlation)*independentNormal;
}

WACOMM_HOST_DEVICE inline DriftSide driftSideFromUniform(double uniformValue,double rightProbability) {
    return uniformValue<rightProbability ? DriftSide::RIGHT : DriftSide::LEFT;
}

inline DriftSide sampleDriftSide(std::uint64_t seed,std::uint64_t particle,double rightProbability) {
    return driftSideFromUniform(NumericalHelpers::uniform(seed,particle,LEEWAY_SIDE_RANDOM_INTERVAL,0,0),
                                rightProbability);
}

WACOMM_HOST_DEVICE inline double jibeStepProbability(double hourlyProbability,double elapsedSeconds) {
    if (hourlyProbability<=0 || elapsedSeconds<=0) return 0;
    if (hourlyProbability>=1) return 1;
    return 1-std::exp(std::log(1-hourlyProbability)*elapsedSeconds/3600);
}

class DriftObjectCatalog {
public:
    static const DriftObjectDefinition& definition(DriftObjectType type);
    static DriftObjectType type(const char *name);
    static const char *name(DriftObjectType type);
};

class DriftProcessCatalog {
public:
    static const DriftProcessDefinition& definition(DriftProcessType type);
    static DriftProcessType type(const char *name);
    static const char *name(DriftProcessType type);
};

WACOMM_HOST_DEVICE inline LeewayCoefficients driftObjectCoefficients(DriftObjectType type) {
    switch (type) {
        case DriftObjectType::PERSON_IN_WATER: return {.0096,0,.12,.0054,0,.094,-.0054,0,.094};
        case DriftObjectType::LIFERAFT_NO_DROGUE: return {.0339,0,.024,.0149,0,.024,-.0149,0,.024};
        case DriftObjectType::LIFERAFT_DROGUE: return {.0121,0,.12,.0092,0,.094,-.0092,0,.094};
        case DriftObjectType::GENERIC_VESSEL: return {.0247,0,.12,.0276,0,.094,-.0276,0,.094};
        case DriftObjectType::SHIPPING_CONTAINER: return {.0125,.0396,.0281,.0019,.0114,.0436,-.0019,-.0114,.0436};
        case DriftObjectType::PERSON_IN_WATER_PFD: return {.0048,0,.083,.0015,0,.067,-.0015,0,.067};
        case DriftObjectType::PERSON_IN_WATER_SURVIVAL_SUIT: return {.0171,.0112,.0393,.0136,-.033,.0171,-.0013,-.0265,.0162};
        case DriftObjectType::PERSON_IN_WATER_DECEASED: return {.01117,.102,.0304,.0004,.039,.0405,-.0004,-.039,.0405};
        case DriftObjectType::LIFERAFT_DEEP_BALLAST: return {.0352,-.025,.061,.0062,-.03,.035,-.0045,-.002,.036};
        case DriftObjectType::LIFERAFT_DEEP_BALLAST_CAPSIZED: return {.0088,0,.025,.0018,0,.024,-.0018,0,.024};
        case DriftObjectType::KAYAK_WITH_PERSON: return {.0116,.1112,.0412,.0041,0,.0439,-.0041,0,.0439};
        case DriftObjectType::SURFBOARD_WITH_PERSON: return {.0193,0,.083,.0051,0,.067,-.0051,0,.067};
        case DriftObjectType::SKIFF: return {.0315,0,.022,.0129,0,.022,-.0129,0,.022};
        case DriftObjectType::SKIFF_CAPSIZED: return {.0165,0,.031,.0039,0,.029,-.0039,0,.029};
        case DriftObjectType::SPORT_BOAT: return {.0654,0,.03,.0219,0,.028,-.0219,0,.028};
        case DriftObjectType::COASTAL_FREIGHTER: return {.0187,0,.083,.0209,0,.067,-.0209,0,.067};
        case DriftObjectType::SAILBOAT: return {.045,0,.194,.0495,0,.1842,-.0282,0,.2495};
        case DriftObjectType::FISHING_VESSEL_DEBRIS: return {.0197,0,.083,.0036,0,.067,-.0036,0,.067};
        case DriftObjectType::OIL_DRUM: return {.0075,.0266,.0283,.0048,.0288,.0392,-.0045,-.0146,.0459};
        case DriftObjectType::WWII_MINE: return {.0107,.0447,.0655,.0041,.0115,.0413,-.0041,-.0115,.0413};
        case DriftObjectType::REFUGEE_RAFT_NO_SAIL: return {.0156,.083,.0153,.00078,.027,.0152,-.00078,-.027,.0152};
        case DriftObjectType::MEDICAL_WASTE: return {.0275,0,.12,.005,0,.094,-.005,0,.094};
        default: return {0,0,0,0,0,0,0,0,0};
    }
}

WACOMM_HOST_DEVICE inline DriftVelocity computeLeeway(const LeewayCoefficients& coefficients, double windU10,
                                   double windV10, DriftSide side, double downwindNormal=0,
                                   double crosswindNormal=0) {
    double speed=std::sqrt(windU10*windU10+windV10*windV10);
    if (speed<1.0e-12 || side==DriftSide::UNDEFINED) return {0,0,0};
    double windDirectionU=windU10/speed;
    double windDirectionV=windV10/speed;
    double downwind=coefficients.downwindSlope*speed+coefficients.downwindIntercept+
                    coefficients.downwindStdDev*downwindNormal;
    bool right=side==DriftSide::RIGHT;
    double crosswind=(right ? coefficients.crosswindSlope : coefficients.leftCrosswindSlope)*speed+
                     (right ? coefficients.crosswindIntercept : coefficients.leftCrosswindIntercept)+
                     (right ? coefficients.crosswindStdDev : -coefficients.leftCrosswindStdDev)*crosswindNormal;
    return {downwind*windDirectionU-crosswind*windDirectionV,
            downwind*windDirectionV+crosswind*windDirectionU,0};
}

#undef WACOMM_HOST_DEVICE

#endif
