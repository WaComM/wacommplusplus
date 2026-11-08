//
// Instantaneous particle releases inside GeoJSON Polygon areas (docs/model.md#polygon-releases).
//

#include "PolygonRelease.hpp"
#include "DriftModel.hpp"
#include "NumericalHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace {

typedef std::pair<double,double> Vertex;

constexpr double DEGREES_TO_RADIANS=3.14159265358979323846/180.0;

// Twice the signed area of the triangle (a, b, c) in the lon/lat plane
double orientation(const Vertex &a, const Vertex &b, const Vertex &c) {
    return (b.first-a.first)*(c.second-a.second)-(b.second-a.second)*(c.first-a.first);
}

// True if c, collinear with a and b, lies on the closed segment [a, b]
bool onSegment(const Vertex &a, const Vertex &b, const Vertex &c) {
    return std::min(a.first,b.first)<=c.first && c.first<=std::max(a.first,b.first) &&
           std::min(a.second,b.second)<=c.second && c.second<=std::max(a.second,b.second);
}

// True if the closed segments [p1, p2] and [p3, p4] share at least one point
bool segmentsIntersect(const Vertex &p1, const Vertex &p2, const Vertex &p3, const Vertex &p4) {
    double d1=orientation(p3,p4,p1), d2=orientation(p3,p4,p2);
    double d3=orientation(p1,p2,p3), d4=orientation(p1,p2,p4);
    if (((d1>0 && d2<0) || (d1<0 && d2>0)) && ((d3>0 && d4<0) || (d3<0 && d4>0))) return true;
    if (d1==0 && onSegment(p3,p4,p1)) return true;
    if (d2==0 && onSegment(p3,p4,p2)) return true;
    if (d3==0 && onSegment(p1,p2,p3)) return true;
    if (d4==0 && onSegment(p1,p2,p4)) return true;
    return false;
}

// Sine on [-pi/2, pi/2] from its Taylor polynomial through x^25 (truncation below 2e-23),
// evaluated with basic IEEE-754 operations only. This translation unit is compiled without
// floating-point contraction, so the result is bitwise identical on every supported platform,
// unlike the C library sin, whose last bits are implementation-defined.
double portableSin(double x) {
    double x2=x*x, r=1.0;
    for (int n=12; n>=1; n--) r=1.0-x2/((2.0*n)*(2.0*n+1.0))*r;
    return x*r;
}

// The latitude [rad] in [low, high] whose portableSin is z, by bisection to adjacent doubles
double portableArcsin(double z, double low, double high) {
    for (int iteration=0; iteration<2200; iteration++) {
        double middle=0.5*(low+high);
        if (middle<=low || middle>=high) break;
        if (portableSin(middle)<z) low=middle; else high=middle;
    }
    return 0.5*(low+high);
}

// Even-odd crossing count of a horizontal ray from (lon, lat) toward +lon
bool crossings(const std::vector<Vertex> &ring, double lon, double lat) {
    bool inside=false;
    for (std::size_t a=0, b=ring.size()-2; a+1<ring.size(); b=a++) {
        const Vertex &va=ring[a], &vb=ring[b];
        if ((va.second>lat)!=(vb.second>lat)) {
            double x=va.first+(lat-va.second)*(vb.first-va.first)/(vb.second-va.second);
            if (lon<x) inside=!inside;
        }
    }
    return inside;
}

}

std::vector<PolygonRelease> PolygonRelease::loadFromJson(const std::string &fileName) {
    std::ifstream input(fileName);
    if (!input) throw std::runtime_error("Cannot open polygon release file: " + fileName);
    json featureCollection;
    try {
        input >> featureCollection;
    } catch (const json::parse_error &e) {
        throw std::runtime_error("Invalid polygon release file " + fileName + ": " + e.what());
    }
    return fromJson(featureCollection);
}

std::vector<PolygonRelease> PolygonRelease::fromJson(const json &featureCollection) {
    if (!featureCollection.is_object() || featureCollection.value("type","")!="FeatureCollection" ||
        !featureCollection.contains("features") || !featureCollection["features"].is_array() ||
        featureCollection["features"].empty())
        throw std::runtime_error("A polygon release file must be a non-empty GeoJSON FeatureCollection");

    std::vector<PolygonRelease> releases;
    std::size_t index=0;
    for (const auto &feature: featureCollection["features"]) {
        PolygonRelease release;
        release.name="feature " + std::to_string(index);
        if (!feature.is_object() || !feature.contains("properties") || !feature["properties"].is_object())
            throw std::runtime_error("Polygon release " + release.name + ": properties are required");
        const json &properties=feature["properties"];
        if (properties.contains("name") && properties["name"].is_string())
            release.name=properties["name"].get<std::string>();

        // Particle count: a positive integer
        if (!properties.contains("particles") || !properties["particles"].is_number_integer() ||
            properties["particles"].get<long long>()<=0)
            throw std::runtime_error("Polygon release " + release.name + ": particles must be a positive integer");
        release.particles=properties["particles"].get<std::uint64_t>();

        // Depth below the free surface [m], positive down
        if (!properties.contains("depth") || !properties["depth"].is_number())
            throw std::runtime_error("Polygon release " + release.name + ": depth [m, positive down] is required");
        release.depth=properties["depth"].get<double>();
        if (!std::isfinite(release.depth) || release.depth<0)
            throw std::runtime_error("Polygon release " + release.name + ": depth must be finite and nonnegative");

        // Geometry: a GeoJSON Polygon, exterior ring followed by holes
        if (!feature.contains("geometry") || !feature["geometry"].is_object() ||
            feature["geometry"].value("type","")!="Polygon" ||
            !feature["geometry"].contains("coordinates") || !feature["geometry"]["coordinates"].is_array() ||
            feature["geometry"]["coordinates"].empty())
            throw std::runtime_error("Polygon release " + release.name + ": geometry must be a GeoJSON Polygon");
        for (const auto &ringJson: feature["geometry"]["coordinates"]) {
            if (!ringJson.is_array() || ringJson.size()<4)
                throw std::runtime_error("Polygon release " + release.name + ": a ring needs at least four positions");
            std::vector<Vertex> ring;
            for (const auto &position: ringJson) {
                // A third coordinate would be ambiguous with the release depth
                if (!position.is_array() || position.size()!=2 ||
                    !position[0].is_number() || !position[1].is_number())
                    throw std::runtime_error("Polygon release " + release.name + ": positions must be [lon, lat]");
                ring.emplace_back(position[0].get<double>(),position[1].get<double>());
            }
            release.rings.push_back(ring);
        }
        release.validate();

        // Bounding box of the exterior ring (holes lie inside it)
        const auto &exterior=release.rings[0];
        release.lonMin=release.lonMax=exterior[0].first;
        release.latMin=release.latMax=exterior[0].second;
        for (const auto &vertex: exterior) {
            release.lonMin=std::min(release.lonMin,vertex.first); release.lonMax=std::max(release.lonMax,vertex.first);
            release.latMin=std::min(release.latMin,vertex.second); release.latMax=std::max(release.latMax,vertex.second);
        }
        releases.push_back(release);
        index++;
    }
    return releases;
}

void PolygonRelease::validate() const {
    for (std::size_t r=0; r<rings.size(); r++) {
        const auto &ring=rings[r];
        for (const auto &vertex: ring) {
            if (!std::isfinite(vertex.first) || !std::isfinite(vertex.second) ||
                vertex.second<-90 || vertex.second>90)
                throw std::runtime_error("Polygon release " + name + ": positions must be finite with -90 <= lat <= 90");
        }
        if (ring.front()!=ring.back())
            throw std::runtime_error("Polygon release " + name + ": rings must be closed (first position == last)");
        std::size_t edges=ring.size()-1;
        for (std::size_t e=0; e<edges; e++) {
            if (ring[e]==ring[e+1])
                throw std::runtime_error("Polygon release " + name + ": rings must not repeat consecutive positions");
        }

        // A simple ring: non-adjacent edges are disjoint, and adjacent edges meet only at their vertex
        for (std::size_t a=0; a<edges; a++) {
            for (std::size_t b=a+1; b<edges; b++) {
                bool adjacent=(b==a+1) || (a==0 && b==edges-1);
                if (adjacent) {
                    const Vertex &shared=(b==a+1) ? ring[b] : ring[a];
                    const Vertex &other1=(b==a+1) ? ring[a] : ring[a+1];
                    const Vertex &other2=(b==a+1) ? ring[b+1] : ring[b];
                    // Collinear adjacent edges folding back on themselves overlap
                    if (orientation(other1,shared,other2)==0 &&
                        (other1.first-shared.first)*(other2.first-shared.first)+
                        (other1.second-shared.second)*(other2.second-shared.second)>0)
                        throw std::runtime_error("Polygon release " + name + ": ring edges overlap");
                    continue;
                }
                if (segmentsIntersect(ring[a],ring[a+1],ring[b],ring[b+1]))
                    throw std::runtime_error("Polygon release " + name + ": rings must not self-intersect");
            }
        }

        // Rings must not touch or cross each other
        for (std::size_t s=0; s<r; s++) {
            const auto &other=rings[s];
            for (std::size_t a=0; a+1<ring.size(); a++)
                for (std::size_t b=0; b+1<other.size(); b++)
                    if (segmentsIntersect(ring[a],ring[a+1],other[b],other[b+1]))
                        throw std::runtime_error("Polygon release " + name + ": rings must not intersect each other");
        }
    }

    const auto &exterior=rings[0];
    double twiceArea=0;
    double lonLow=exterior[0].first, lonHigh=exterior[0].first;
    for (std::size_t e=0; e+1<exterior.size(); e++) {
        twiceArea+=exterior[e].first*exterior[e+1].second-exterior[e+1].first*exterior[e].second;
        lonLow=std::min(lonLow,exterior[e].first); lonHigh=std::max(lonHigh,exterior[e].first);
    }
    if (twiceArea==0)
        throw std::runtime_error("Polygon release " + name + ": the exterior ring has zero area");
    // Edges are straight in lon/lat; a ring spanning 180 degrees or more is antimeridian-ambiguous
    if (lonHigh-lonLow>=180)
        throw std::runtime_error("Polygon release " + name + ": the exterior ring spans 180 degrees of longitude or more");

    // Holes must lie inside the exterior ring; nesting of holes is not supported
    for (std::size_t r=1; r<rings.size(); r++) {
        if (!crossings(exterior,rings[r][0].first,rings[r][0].second))
            throw std::runtime_error("Polygon release " + name + ": holes must lie inside the exterior ring");
        for (std::size_t s=1; s<rings.size(); s++)
            if (s!=r && crossings(rings[s],rings[r][0].first,rings[r][0].second))
                throw std::runtime_error("Polygon release " + name + ": holes must not be nested");
    }
}

const std::string &PolygonRelease::Name() const { return name; }
std::uint64_t PolygonRelease::Particles() const { return particles; }
double PolygonRelease::Depth() const { return depth; }

bool PolygonRelease::contains(double lon, double lat) const {
    if (!crossings(rings[0],lon,lat)) return false;
    for (std::size_t r=1; r<rings.size(); r++)
        if (crossings(rings[r],lon,lat)) return false;
    return true;
}

std::uint64_t PolygonRelease::position(std::uint64_t seed, std::uint64_t particle, double &lon, double &lat) const {
    // Uniform per unit spherical area: dA = R^2 cos(lat) dlat dlon, so lon and sin(lat)
    // are uniform on the bounding box, and candidates outside the polygon are rejected.
    double radLow=latMin*DEGREES_TO_RADIANS, radHigh=latMax*DEGREES_TO_RADIANS;
    double sinLow=portableSin(radLow), sinHigh=portableSin(radHigh);
    for (std::uint64_t candidate=0; candidate<MAX_CANDIDATES; candidate++) {
        double uLon=NumericalHelpers::uniform(seed,particle,POLYGON_RELEASE_RANDOM_INTERVAL,candidate,0);
        double uLat=NumericalHelpers::uniform(seed,particle,POLYGON_RELEASE_RANDOM_INTERVAL,candidate,1);
        lon=lonMin+uLon*(lonMax-lonMin);
        lat=std::max(latMin,std::min(latMax,portableArcsin(sinLow+uLat*(sinHigh-sinLow),radLow,radHigh)/DEGREES_TO_RADIANS));
        if (contains(lon,lat)) return candidate+1;
    }
    throw std::runtime_error("Polygon release " + name + ": no sample inside the polygon after " +
                             std::to_string(MAX_CANDIDATES) + " candidates");
}

void PolygonRelease::emit(const std::vector<PolygonRelease> &releases, const std::shared_ptr<Config> &config,
                          OceanModelAdapter &oceanModelAdapter, double releaseTime, ::Particles &particles) {
    if (!std::isfinite(releaseTime))
        throw std::runtime_error("The polygon release time must be finite");

    size_t eta_rho = oceanModelAdapter.Mask().Nx();
    size_t xi_rho = oceanModelAdapter.Mask().Ny();

    for (const auto &release: releases) {
        for (std::uint64_t n=0; n<release.particles; n++) {
            // Identities continue the existing sequence, as for source emission
            std::uint64_t id=particles.size()==0 ? 0 : particles.at(particles.size()-1).Id()+1;

            double lon, lat, k, j, i;
            release.position(config->RandomSeed(),id,lon,lat);
            try {
                oceanModelAdapter.deplatlon2kji(release.depth,lat,lon,k,j,i);
            } catch (const std::exception &e) {
                throw std::runtime_error("Polygon release " + release.name + ", particle " +
                                         std::to_string(id) + ": " + e.what());
            }

            // The four rho nodes of the interpolation cell must be wet
            int jI=NumericalHelpers::horizontalCell(j), iI=NumericalHelpers::horizontalCell(i);
            if (!NumericalHelpers::validInterpolationCell(jI,iI,eta_rho,xi_rho) ||
                oceanModelAdapter.Mask()(jI,iI)!=1 || oceanModelAdapter.Mask()(jI+1,iI)!=1 ||
                oceanModelAdapter.Mask()(jI+1,iI+1)!=1 || oceanModelAdapter.Mask()(jI,iI+1)!=1)
                throw std::runtime_error("Polygon release " + release.name + ", particle " +
                                         std::to_string(id) + ": lat=" + std::to_string(lat) +
                                         " lon=" + std::to_string(lon) +
                                         " is in an interpolation cell with a land node");

            Particle particle(id, k, j, i, releaseTime);
            DriftSide side=config->DefaultDriftSide();
            if (config->LeewayRandomSide())
                side=sampleDriftSide(config->RandomSeed(),id,config->LeewayRightSideProbability());
            particle.Drift(config->DriftObject(),side);
            particles.push_back(particle);
        }
    }
}
