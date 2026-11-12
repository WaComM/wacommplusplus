// Regression test for polygon releases (docs/model.md#polygon-releases).
//
// On a fixture grid, a concave polygon with a hole is sampled and every particle must
// map back (kji2deplatlon) inside the polygon at the release depth, with the indices
// that deplatlon2kji assigns, the configured count, and consecutive unsigned 64-bit
// identities. The cloud must be identical for the same seed, differ for another seed,
// and be keyed by identity rather than by rejection history; golden positions must be
// reproduced bit for bit, as on every supported platform. The sampling density must
// be uniform per unit spherical area, and invalid geometry, out-of-domain, and land
// positions must fail. The cloud must round-trip exactly through the production restart,
// and the oil-spill example polygons (argument: the examples directory) must validate.

#include "../OceanModelAdapter.hpp"
#include "../Particles.hpp"
#include "../NumericalHelpers.hpp"
#include "../PolygonRelease.hpp"

#include <netcdf>
#include <nlohmann/json.hpp>

#include <cassert>
#include <cmath>
#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

class FixtureAdapter: public OceanModelAdapter {
public:
    void process() override {}
};

static const double PI=3.14159265358979323846;
static const double LON0=14.0, LAT0=40.5, DLON=0.01, DLAT=0.01;
static const int NI=21, NJ=21;
static const double DEPTH=10.0; // m, positive down

static std::shared_ptr<FixtureAdapter> fixture() {
    auto adapter=std::make_shared<FixtureAdapter>();
    adapter->OceanTime().Allocate(2); adapter->OceanTime()(0)=0; adapter->OceanTime()(1)=3600;
    adapter->SRho().Allocate(2,-1); adapter->SRho()(-1)=-.75; adapter->SRho()(0)=-.25;
    adapter->SW().Allocate(3,-2); adapter->SW()(-2)=-1; adapter->SW()(-1)=-.5; adapter->SW()(0)=0;
    adapter->W().Allocate(2,3,NJ,NI,0,-2,0,0); adapter->W()=0.0f;
    adapter->Mask().Allocate(NJ,NI); adapter->Mask()=1.0;
    adapter->Lon().Allocate(NJ,NI); adapter->Lat().Allocate(NJ,NI); adapter->H().Allocate(NJ,NI);
    for (int j=0;j<NJ;j++) for (int i=0;i<NI;i++) {
        adapter->Lon()(j,i)=LON0+i*DLON; adapter->Lat()(j,i)=LAT0+j*DLAT; adapter->H()(j,i)=100;
    }
    // One land node in the north-east corner
    adapter->Mask()(NJ-2,NI-2)=0;
    return adapter;
}

static json polygon(const json& rings, json properties) {
    return {{"type","FeatureCollection"},{"features",json::array({{
        {"type","Feature"},{"properties",properties},
        {"geometry",{{"type","Polygon"},{"coordinates",rings}}}}})}};
}

// Concave exterior (a notch on the north side) with a square hole
static json concaveWithHole(int particles) {
    json exterior={{14.03,40.53},{14.12,40.53},{14.12,40.61},{14.08,40.57},{14.05,40.61},{14.03,40.61},{14.03,40.53}};
    json hole={{14.04,40.54},{14.06,40.54},{14.06,40.56},{14.04,40.56},{14.04,40.54}};
    return polygon({exterior,hole},{{"name","concave"},{"particles",particles},{"depth",DEPTH}});
}

static std::shared_ptr<Config> configuration(std::uint64_t seed) {
    const std::string fileName="polygon-release-config.json";
    std::ofstream(fileName) << json({{"io",{{"ocean_model","WACOMM"}}},{"physics",{{"random_seed",seed}}},
                                     {"tracking",{{"direction","forward"}}}}).dump();
    return std::make_shared<Config>(fileName);
}

// True if parsing fails with a message containing reason
static bool rejected(const json& featureCollection, const std::string& reason="") {
    try { PolygonRelease::fromJson(featureCollection); }
    catch (const std::runtime_error& e) { return std::string(e.what()).find(reason)!=std::string::npos; }
    return false;
}

static bool emitFails(const json& featureCollection, FixtureAdapter& adapter, const std::string& reason) {
    Particles particles;
    try { PolygonRelease::emit(PolygonRelease::fromJson(featureCollection),configuration(5489),adapter,0,particles); }
    catch (const std::runtime_error& e) { return std::string(e.what()).find(reason)!=std::string::npos; }
    return false;
}

int main(int argc, char** argv) {
    // The checked-in oil-spill release polygons must pass validation
    if (argc==2) {
        for (const std::string name: {"oilspill-demo-01","oilspill-demo-02"}) {
            auto demo=PolygonRelease::loadFromJson(std::string(argv[1])+"/"+name+"/"+name+".json");
            assert(demo.size()==1 && demo[0].Particles()==10000 && demo[0].Depth()==0);
        }
    }

    auto adapter=fixture();
    const int N=2000;
    const double releaseTime=1800;

    auto releases=PolygonRelease::fromJson(concaveWithHole(N));
    assert(releases.size()==1 && releases[0].Particles()==(std::uint64_t)N);
    // Membership: notch, hole, and interior
    assert(!releases[0].contains(14.08,40.60));
    assert(!releases[0].contains(14.05,40.55));
    assert(releases[0].contains(14.10,40.55));

    Particles particles;
    PolygonRelease::emit(releases,configuration(5489),*adapter,releaseTime,particles);
    assert((int)particles.size()==N);
    for (int n=0;n<N;n++) {
        Particle& particle=particles.at(n);
        assert(particle.Id()==(std::uint64_t)n);
        assert(particle.Time()==releaseTime && particle.Age()==0);

        // Containment and depth after the forward map of the solver
        double dep,lat,lon;
        adapter->kji2deplatlon(particle.K(),particle.J(),particle.I(),dep,lat,lon);
        assert(releases[0].contains(lon,lat));
        assert(std::abs(dep+DEPTH)<1e-9);

        // The indices are those of the exact inverse
        double k,j,i;
        adapter->deplatlon2kji(DEPTH,lat,lon,k,j,i);
        assert(std::abs(k-particle.K())<1e-12 && std::abs(j-particle.J())<1e-9 && std::abs(i-particle.I())<1e-9);
        assert(std::abs(particle.K()-(-DEPTH/(100*0.5)))<1e-12);
    }

    // Seeded reproducibility: the same seed reproduces the cloud bit for bit
    Particles again;
    PolygonRelease::emit(releases,configuration(5489),*adapter,releaseTime,again);
    for (int n=0;n<N;n++)
        assert(again.at(n).I()==particles.at(n).I() && again.at(n).J()==particles.at(n).J() &&
               again.at(n).K()==particles.at(n).K());

    // Another seed gives another cloud
    Particles other;
    PolygonRelease::emit(releases,configuration(5490),*adapter,releaseTime,other);
    int same=0;
    for (int n=0;n<N;n++) if (other.at(n).I()==particles.at(n).I()) same++;
    assert(same==0);

    // Keyed by identity: a smaller release is a prefix of the larger one
    Particles prefix;
    PolygonRelease::emit(PolygonRelease::fromJson(concaveWithHole(100)),configuration(5489),*adapter,releaseTime,prefix);
    for (int n=0;n<100;n++) assert(prefix.at(n).I()==particles.at(n).I() && prefix.at(n).J()==particles.at(n).J());

    // Two features continue the identity sequence
    json two=concaveWithHole(3);
    two["features"].push_back(two["features"][0]);
    two["features"][1]["properties"]["particles"]=4;
    Particles sequence;
    PolygonRelease::emit(PolygonRelease::fromJson(two),configuration(5489),*adapter,releaseTime,sequence);
    assert(sequence.size()==7 && sequence.at(6).Id()==6);

    // Uniform per unit spherical area: the fraction south of 30N in a 0-60N box is
    // sin(30)/sin(60) = 0.577 (0.5 for a uniform latitude); 5 binomial standard deviations
    {
        auto band=PolygonRelease::fromJson(polygon(json::array({json{{0,0},{10,0},{10,60},{0,60},{0,0}}}),
                                                   {{"particles",1},{"depth",0}}))[0];
        const int samples=20000;
        int south=0;
        for (int n=0;n<samples;n++) {
            double lon,lat;
            band.position(5489,n,lon,lat);
            assert(lon>=0 && lon<=10 && lat>=0 && lat<=60);
            if (lat<30) south++;
        }
        // Golden positions [deg] for seed 5489: the sampler uses only basic IEEE-754 operations
        // without contraction, so these bits must be reproduced on every supported platform
        const double golden[4][2]={{0x1.b86d2772642bap+1,0x1.9d7474662e662p+5},{0x1.3a7ef4c5ffbb5p+3,0x1.c5362c37bd5fcp+5},
                                   {0x1.3ecc3394815cep+3,0x1.8fa0d7e90a22dp+5},{0x1.f33fa65faa796p+1,0x1.4e10323038003p+5}};
        for (int n=0;n<4;n++) {
            double lon,lat;
            band.position(5489,n,lon,lat);
            assert(lon==golden[n][0] && lat==golden[n][1]);
            // The portable inverse agrees with the C library within round-off
            double uLat=NumericalHelpers::uniform(5489,n,POLYGON_RELEASE_RANDOM_INTERVAL,0,1);
            double reference=std::asin(uLat*std::sin(PI/3))*180.0/PI;
            assert(std::abs(lat-reference)<1e-12);
        }
        double expected=std::sin(PI/6)/std::sin(PI/3);
        double sigma=std::sqrt(expected*(1-expected)/samples);
        assert(std::abs((double)south/samples-expected)<5*sigma);
    }

    // Invalid geometry and properties fail
    json square={{14.03,40.53},{14.05,40.53},{14.05,40.55},{14.03,40.55},{14.03,40.53}};
    json properties={{"particles",10},{"depth",0}};
    assert(!rejected(polygon(json::array({square}),properties)));
    json point=polygon(json::array({square}),properties);
    point["features"][0]["geometry"]={{"type","Point"},{"coordinates",{14.04,40.54}}};
    assert(rejected(point,"geometry must be a GeoJSON Polygon"));
    json multi=polygon(json::array({square}),properties);
    multi["features"][0]["geometry"]["type"]="MultiPolygon";
    assert(rejected(multi,"geometry must be a GeoJSON Polygon"));
    assert(rejected(polygon(json::array({json{{14.03,40.53},{14.05,40.53},{14.05,40.55},{14.03,40.55}}}),properties),"closed"));
    assert(rejected(polygon(json::array({json{{14.03,40.53},{14.05,40.55},{14.05,40.53},{14.03,40.55},{14.03,40.53}}}),properties),"self-intersect"));
    assert(rejected(polygon(json::array({json{{14.03,40.53},{14.04,40.53},{14.05,40.53},{14.03,40.53}}}),properties),"overlap"));
    assert(rejected(polygon(json::array({json{{14.03,40.53,0},{14.05,40.53,0},{14.05,40.55,0},{14.03,40.53,0}}}),properties),"[lon, lat]"));
    assert(rejected(polygon(json::array({json{{-100,0},{100,0},{100,10},{-100,0}}}),properties),"180 degrees"));
    json outsideHole={{14.10,40.60},{14.11,40.60},{14.11,40.61},{14.10,40.60}};
    assert(rejected(polygon({square,outsideHole},properties),"inside the exterior"));
    assert(rejected(polygon(json::array({square}),{{"particles",0},{"depth",0}}),"positive integer"));
    assert(rejected(polygon(json::array({square}),{{"particles",1.5},{"depth",0}}),"positive integer"));
    assert(rejected(polygon(json::array({square}),{{"particles",10},{"depth",-1}}),"nonnegative"));
    assert(rejected(polygon(json::array({square}),{{"particles",10}}),"depth [m, positive down] is required"));
    assert(rejected(json::object({{"type","FeatureCollection"},{"features",json::array()}}),"non-empty"));

    // Positions outside the interpolation domain or in a cell with a land node fail
    assert(emitFails(polygon(json::array({json{{13.90,40.53},{14.01,40.53},{14.01,40.55},{13.90,40.55},{13.90,40.53}}}),properties),
                     *adapter,"outside the ocean model interpolation domain"));
    assert(emitFails(polygon(json::array({json{{14.185,40.685},{14.195,40.685},{14.195,40.695},{14.185,40.685}}}),properties),
                     *adapter,"land node"));

    // Exact round trip through the production restart writer and loader
    {
        auto config=configuration(5489);
        const std::string restartFile="polygon-release-restart.nc";
        particles.saveAsNetCDF(restartFile,releaseTime,adapter,config);
        {
            netCDF::NcFile file(restartFile,netCDF::NcFile::read);
            std::string version;
            file.getAtt("wacomm_restart_version").getValues(version);
            assert(version=="3");
            std::uint64_t seed;
            file.getAtt("random_seed").getValues(&seed);
            assert(seed==5489);
            double checkpoint;
            file.getAtt("checkpoint_time").getValues(&checkpoint);
            assert(checkpoint==releaseTime);
            assert(file.getVar("id").getType()==netCDF::ncUint64);
        }
        Particles loaded;
        loaded.loadFromNetCDF(restartFile,config);
        assert(loaded.size()==particles.size());
        for (int n=0;n<N;n++) {
            Particle &a=particles.at(n), &b=loaded.at(n);
            assert(a.Id()==b.Id() && a.K()==b.K() && a.J()==b.J() && a.I()==b.I() &&
                   a.Time()==b.Time() && a.Age()==b.Age() && a.Health()==b.Health());
        }
        assert(config->RestartCheckpoint()==releaseTime);
    }
    return 0;
}
