#ifndef WACOMMPLUSPLUS_SOURCEEMISSION_HPP
#define WACOMMPLUSPLUS_SOURCEEMISSION_HPP

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "NumericalHelpers.hpp"

enum class EmissionMode {
    UNIFORM_RATE,
    FORCING_INTERVAL_BATCH,
    SINGLE_PULSE
};

static constexpr std::int64_t SOURCE_POSITION_RANDOM_INTERVAL=(-9223372036854775807LL-1)+2;

inline double sourcePositionOffset(std::uint64_t seed, std::uint64_t particle,
                                   std::uint64_t component) {
    return 0.25*NumericalHelpers::normal(seed,particle,SOURCE_POSITION_RANDOM_INTERVAL,0,component);
}

inline EmissionMode emissionModeFromString(const std::string& value) {
    if (value=="uniform_rate") return EmissionMode::UNIFORM_RATE;
    if (value=="forcing_interval_batch") return EmissionMode::FORCING_INTERVAL_BATCH;
    if (value=="single_pulse") return EmissionMode::SINGLE_PULSE;
    throw std::runtime_error("unsupported source emission mode: " + value);
}

inline std::string emissionModeToString(EmissionMode value) {
    if (value==EmissionMode::UNIFORM_RATE) return "uniform_rate";
    if (value==EmissionMode::FORCING_INTERVAL_BATCH) return "forcing_interval_batch";
    return "single_pulse";
}

inline std::vector<double> sourceEmissionTimes(EmissionMode mode, double value,
                                               double sourceStart, double sourceEnd,
                                               double intervalStart, double intervalEnd) {
    std::vector<double> times;
    if (!std::isfinite(value) || value<0 || !std::isfinite(sourceStart) ||
        !std::isfinite(intervalStart) || !std::isfinite(intervalEnd) || intervalEnd<=intervalStart)
        throw std::runtime_error("invalid source emission schedule");

    if (value==0) return times;

    if (mode==EmissionMode::SINGLE_PULSE) {
        if (std::floor(value)!=value)
            throw std::runtime_error("single_pulse requires an integer particles");
        if (sourceStart>=intervalStart && sourceStart<intervalEnd)
            times.assign((std::size_t)value,sourceStart);
        return times;
    }

    double activeStart=std::max(intervalStart,sourceStart);
    double activeEnd=sourceEnd<0 ? intervalEnd : std::min(intervalEnd,sourceEnd);
    if (activeEnd<=activeStart) return times;

    if (mode==EmissionMode::FORCING_INTERVAL_BATCH) {
        if (std::floor(value)!=value)
            throw std::runtime_error("forcing_interval_batch requires an integer particles_per_interval");
        if (intervalStart>=sourceStart && (sourceEnd<0 || intervalStart<sourceEnd))
            times.assign((std::size_t)value,intervalStart);
        return times;
    }
    double spacing=3600.0/value;
    long long first=std::max(0LL,(long long)std::ceil((activeStart-sourceStart)/spacing-0.5));
    for (long long index=first;;index++) {
        double time=sourceStart+(index+0.5)*spacing;
        if (time>=activeEnd) break;
        if (time>=activeStart) times.push_back(time);
    }
    return times;
}

#endif //WACOMMPLUSPLUS_SOURCEEMISSION_HPP
