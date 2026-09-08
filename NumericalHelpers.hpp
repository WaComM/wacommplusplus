#ifndef WACOMMPLUSPLUS_NUMERICALHELPERS_HPP
#define WACOMMPLUSPLUS_NUMERICALHELPERS_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace NumericalHelpers {

inline bool validInterpolationCell(int j, int i, std::size_t eta, std::size_t xi) {
    return j >= 0 && i >= 0 && static_cast<std::size_t>(j + 1) < eta && static_cast<std::size_t>(i + 1) < xi;
}

inline std::size_t storageLevel(int logicalLevel, std::size_t levels) {
    return static_cast<std::size_t>(logicalLevel + static_cast<int>(levels) - 1);
}

inline double timeWeight(double time, double time0, double time1) {
    if (time1 == time0) return 0.0;
    return std::max(0.0, std::min(1.0, (time - time0) / (time1 - time0)));
}

inline double interpolateTime(double value0, double value1, double alpha) {
    return (1.0 - alpha) * value0 + alpha * value1;
}

inline double stepSize(double current, double end, double maximumStep) {
    return std::copysign(std::min(std::abs(end - current), maximumStep), end - current);
}

inline double restartElapsed(double intervalStart, double intervalEnd, double checkpoint) {
    if (intervalEnd>intervalStart) {
        if (intervalEnd<=checkpoint) return std::numeric_limits<double>::quiet_NaN();
        return std::min(intervalEnd-intervalStart,std::max(0.0,checkpoint-intervalStart));
    }
    if (intervalEnd>=checkpoint) return std::numeric_limits<double>::quiet_NaN();
    return std::min(intervalStart-intervalEnd,std::max(0.0,intervalStart-checkpoint));
}

inline void reflectCell(double oldCoordinate, int oldCell, double &candidate, int candidateCell) {
    if (candidateCell < oldCell) candidate = oldCell + std::abs(oldCoordinate - candidate);
    else if (candidateCell > oldCell) candidate = candidateCell - std::fmod(candidate, 1.0);
}

inline std::uint64_t mix(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

inline double uniform(std::uint64_t seed, std::uint64_t particle, std::int64_t interval,
                      std::uint64_t substep, std::uint64_t component) {
    std::uint64_t key = mix(seed) ^ mix(particle) ^ mix(static_cast<std::uint64_t>(interval));
    key ^= mix(substep) ^ mix(component);
    return (mix(key) >> 11U) * 0x1.0p-53;
}

inline double normal(std::uint64_t seed, std::uint64_t particle, std::int64_t interval,
                     std::uint64_t substep, std::uint64_t component) {
    double u1 = std::max(uniform(seed, particle, interval, substep, component * 2),
                         std::numeric_limits<double>::min());
    double u2 = uniform(seed, particle, interval, substep, component * 2 + 1);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.28318530717958647692 * u2);
}

}

#endif
