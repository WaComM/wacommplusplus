//
// Instantaneous particle releases inside GeoJSON Polygon areas.
//
// A release samples a configured number of particles inside a polygon, at one depth and
// one physical time, and maps each position onto the grid with the same exact inverse used
// by Point sources (OceanModelAdapter::deplatlon2kji). The positions are drawn with the
// stateless seeded generator of NumericalHelpers, keyed by particle identity, so the cloud
// is reproducible from physics.random_seed and independent of rejection history.
// See docs/model.md#polygon-releases.
//

#ifndef WACOMMPLUSPLUS_POLYGONRELEASE_HPP
#define WACOMMPLUSPLUS_POLYGONRELEASE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "Config.hpp"
#include "OceanModelAdapter.hpp"
#include "Particles.hpp"

// Reserved interval coordinate of the release-position random stream; the solver uses
// forcing interval keys and the other reserved streams use the lowest int64 values.
static constexpr std::int64_t POLYGON_RELEASE_RANDOM_INTERVAL=(-9223372036854775807LL-1)+3;

// Identifier of the sampling scheme, archived with the generated restart.
static constexpr const char* POLYGON_RELEASE_SAMPLER = "polygon-rejection-spherical-area-v1";

class PolygonRelease {
public:
    // Parse a GeoJSON FeatureCollection of Polygon features; invalid input throws
    static std::vector<PolygonRelease> loadFromJson(const std::string &fileName);
    static std::vector<PolygonRelease> fromJson(const nlohmann::json &featureCollection);

    const std::string &Name() const;
    std::uint64_t Particles() const;
    double Depth() const;

    // Even-odd membership of (lon, lat) in the exterior ring minus the holes
    bool contains(double lon, double lat) const;

    // Position of the particle-th sample, uniform per unit spherical area inside the polygon;
    // returns the number of candidates drawn
    std::uint64_t position(std::uint64_t seed, std::uint64_t particle, double &lon, double &lat) const;

    // Sample every release at releaseTime [s since 1968-05-23 00:00:00 GMT] into particles
    static void emit(const std::vector<PolygonRelease> &releases, const std::shared_ptr<Config> &config,
                     OceanModelAdapter &oceanModelAdapter, double releaseTime,
                     ::Particles &particles);

    // Upper bound on candidates per particle before the release fails
    static constexpr std::uint64_t MAX_CANDIDATES = 1000000;

private:
    std::string name;
    std::uint64_t particles=0;
    double depth=0;
    // rings[0] is the exterior, the others are holes; each ring is closed, as (lon, lat)
    std::vector<std::vector<std::pair<double,double>>> rings;
    double lonMin=0, lonMax=0, latMin=0, latMax=0;

    void validate() const;
};

#endif //WACOMMPLUSPLUS_POLYGONRELEASE_HPP
