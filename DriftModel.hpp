#ifndef WACOMMPLUSPLUS_DRIFTMODEL_HPP
#define WACOMMPLUSPLUS_DRIFTMODEL_HPP

#include <cstdint>
#include <cmath>

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
    SHIPPING_CONTAINER = 5
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
};

struct DriftObjectDefinition {
    DriftObjectType type;
    const char *name;
    LeewayCoefficients leeway;
};

struct DriftVelocity {
    double u;
    double v;
    double w;
};

class DriftObjectCatalog {
public:
    static const DriftObjectDefinition& definition(DriftObjectType type);
    static DriftObjectType type(const char *name);
    static const char *name(DriftObjectType type);
};

WACOMM_HOST_DEVICE inline LeewayCoefficients driftObjectCoefficients(DriftObjectType type) {
    switch (type) {
        case DriftObjectType::PERSON_IN_WATER: return {.0096,0,.12,.0054,0,.094};
        case DriftObjectType::LIFERAFT_NO_DROGUE: return {.0339,0,.024,.0149,0,.024};
        case DriftObjectType::LIFERAFT_DROGUE: return {.0121,0,.12,.0092,0,.094};
        case DriftObjectType::GENERIC_VESSEL: return {.0247,0,.12,.0276,0,.094};
        case DriftObjectType::SHIPPING_CONTAINER: return {.0125,.0396,.0281,.0019,.0114,.0436};
        default: return {0,0,0,0,0,0};
    }
}

WACOMM_HOST_DEVICE inline DriftVelocity computeLeeway(const LeewayCoefficients& coefficients, double windU10,
                                   double windV10, DriftSide side) {
    double speed=std::sqrt(windU10*windU10+windV10*windV10);
    if (speed<1.0e-12 || side==DriftSide::UNDEFINED) return {0,0,0};
    double windDirectionU=windU10/speed;
    double windDirectionV=windV10/speed;
    double downwind=coefficients.downwindSlope*speed+coefficients.downwindIntercept;
    double crosswind=coefficients.crosswindSlope*speed+coefficients.crosswindIntercept;
    double orientation=static_cast<int>(side);
    return {downwind*windDirectionU-orientation*crosswind*windDirectionV,
            downwind*windDirectionV+orientation*crosswind*windDirectionU,0};
}

#undef WACOMM_HOST_DEVICE

#endif
