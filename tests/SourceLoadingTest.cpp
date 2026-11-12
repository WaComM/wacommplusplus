// Regression test for GeoJSON source positions (docs/configuration.md#source-position).
// A source is placed by explicit j and i, or by a Point geometry; its depth comes from
// the "depth" property (written by saveAsJson), the deprecated "dep" alias, or a third
// Point coordinate. Missing, partial, or contradictory positions must fail loading.

#include "../Sources.hpp"
#include "../OceanModelAdapter.hpp"

#include <nlohmann/json.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>

using json = nlohmann::json;

class FixtureAdapter: public OceanModelAdapter {
public:
    void process() override {}
};

static const double depthTolerance=1e-9;   // m
static const double indexTolerance=1e-9;   // dimensionless k, j, i
static const string fileName="source-loading-test.json";

static json feature(const json& properties, const json& geometry) {
    json value={{"type","Feature"},{"properties",properties}};
    if (!geometry.is_null()) value["geometry"]=geometry;
    return value;
}

static json point(const json& coordinates) {
    return {{"type","Point"},{"coordinates",coordinates}};
}

static Sources load(const json& properties, const json& geometry, shared_ptr<OceanModelAdapter> adapter) {
    json collection={{"type","FeatureCollection"},{"features",json::array({feature(properties,geometry)})}};
    std::ofstream(fileName) << collection.dump();
    Sources sources;
    string name=fileName;
    sources.loadFromJson(name,adapter);
    return sources;
}

static bool rejected(const json& properties, const json& geometry, shared_ptr<OceanModelAdapter> adapter) {
    try { load(properties,geometry,adapter); }
    catch (const std::runtime_error&) { return true; }
    return false;
}

static double depthOf(Sources& sources, shared_ptr<OceanModelAdapter> adapter) {
    double dep,lat,lon;
    adapter->kji2deplatlon(sources[0].K(),sources[0].J(),sources[0].I(),dep,lat,lon);
    return -dep;
}

int main() {
    // Rectilinear 3x3 grid, h=100 m, W levels at 0, 10, 30, 60, 100 m
    auto adapter=std::make_shared<FixtureAdapter>();
    const int nW=5;
    const double sW[nW]={-1.0,-0.6,-0.3,-0.1,0.0};
    adapter->SW().Allocate(nW,-nW+1);
    for (int k=0;k<nW;k++) adapter->SW()(k-nW+1)=sW[k];
    adapter->W().Allocate(1,nW,3,3,0,-nW+1,0,0); adapter->W()=0.0f;
    adapter->Mask().Allocate(3,3); adapter->Mask()=1.0;
    adapter->Lon().Allocate(3,3); adapter->Lat().Allocate(3,3); adapter->H().Allocate(3,3);
    adapter->LonRad().Allocate(3,3); adapter->LatRad().Allocate(3,3);
    for (int j=0;j<3;j++) for (int i=0;i<3;i++) {
        adapter->Lon()(j,i)=14+0.1*i; adapter->Lat()(j,i)=40+0.1*j; adapter->H()(j,i)=100;
        adapter->LonRad()(j,i)=0.0174533*adapter->Lon()(j,i);
        adapter->LatRad()(j,i)=0.0174533*adapter->Lat()(j,i);
    }
    const json lonLat={14.05,40.15};
    const json base={{"id","s"},{"emission",{{"mode","uniform_rate"},{"rate",10},{"rate_unit","particles/hour"}}}};
    auto with=[&](const json& extra) { json p=base; if (!extra.is_null()) p.update(extra); return p; };

    // "depth" property with a two-coordinate Point; the sign is not significant
    for (double depth: {-20.0,20.0}) {
        auto sources=load(with({{"depth",depth}}),point(lonLat),adapter);
        assert(sources.size()==1);
        assert(std::abs(depthOf(sources,adapter)-20)<=depthTolerance);
        assert(std::abs(sources[0].J()-1.5)<=indexTolerance && std::abs(sources[0].I()-0.5)<=indexTolerance);
    }

    // Deprecated "dep" alias, third Point coordinate, and agreeing combinations
    for (const json& properties: {with({{"dep",20}}),with({{"depth",20},{"dep",-20}}),with({{"depth",-20}})}) {
        auto sources=load(properties,point({14.05,40.15,-20}),adapter);
        assert(std::abs(depthOf(sources,adapter)-20)<=depthTolerance);
    }
    {
        auto sources=load(with({}),point({14.05,40.15,-20}),adapter);
        assert(std::abs(depthOf(sources,adapter)-20)<=depthTolerance);
    }

    // No depth means the surface
    {
        auto sources=load(with({}),point(lonLat),adapter);
        assert(sources[0].K()==0);
    }

    // Contradictory depths
    assert(rejected(with({{"depth",20},{"dep",30}}),point(lonLat),adapter));
    assert(rejected(with({{"depth",0}}),point({14.05,40.15,-20}),adapter));
    assert(rejected(with({{"depth","20"}}),point(lonLat),adapter));

    // Missing or unsupported positions
    assert(rejected(with({}),nullptr,adapter));
    assert(rejected(with({{"depth",0}}),
                    {{"type","Polygon"},{"coordinates",{{{14.0,40.0},{14.1,40.0},{14.1,40.1},{14.0,40.0}}}}},adapter));
    assert(rejected(with({}),point({14.05}),adapter));
    assert(rejected(with({}),point({14.05,40.15,-20,1}),adapter));
    assert(rejected(with({}),point({14.05,"40.15"}),adapter));
    assert(rejected(with({}),point({16.0,42.0}),adapter));

    // Partial explicit indices, or k without j and i
    assert(rejected(with({{"j",1.5}}),point(lonLat),adapter));
    assert(rejected(with({{"i",0.5}}),nullptr,adapter));
    assert(rejected(with({{"k",-1.5}}),point(lonLat),adapter));

    // Explicit indices; a depth without k sets k, a depth with k must agree with it
    {
        auto sources=load(with({{"j",1.25},{"i",0.75}}),nullptr,adapter);
        assert(sources[0].K()==0 && sources[0].J()==1.25 && sources[0].I()==0.75);
    }
    {
        auto sources=load(with({{"j",1.25},{"i",0.75},{"depth",-45}}),nullptr,adapter);
        assert(sources[0].J()==1.25 && sources[0].I()==0.75);
        assert(std::abs(sources[0].K()+2.5)<=indexTolerance);
        assert(std::abs(depthOf(sources,adapter)-45)<=depthTolerance);
    }
    {
        auto sources=load(with({{"k",-2.5},{"j",1.25},{"i",0.75},{"depth",45}}),point(lonLat),adapter);
        assert(sources[0].K()==-2.5 && sources[0].J()==1.25 && sources[0].I()==0.75);
    }
    assert(rejected(with({{"k",-2.5},{"j",1.25},{"i",0.75},{"depth",10}}),nullptr,adapter));
    assert(rejected(with({{"k",0},{"j",2.5},{"i",0.75},{"depth",0}}),nullptr,adapter));
    assert(rejected(with({{"j",2.5},{"i",0.75}}),nullptr,adapter));
    assert(rejected(with({{"j",-0.5},{"i",0.75}}),nullptr,adapter));

    // saveAsJson -> loadFromJson reproduces a subsurface source
    {
        auto sources=load(with({{"depth",-37.5}}),point(lonLat),adapter);
        string saved="source-loading-saved.json";
        sources.saveAsJson(saved,adapter);
        Sources reloaded;
        reloaded.loadFromJson(saved,adapter);
        assert(reloaded.size()==1);
        assert(std::abs(reloaded[0].K()-sources[0].K())<=indexTolerance);
        assert(std::abs(reloaded[0].J()-sources[0].J())<=indexTolerance);
        assert(std::abs(reloaded[0].I()-sources[0].I())<=indexTolerance);
        assert(std::abs(depthOf(reloaded,adapter)-37.5)<=depthTolerance);
        std::remove(saved.c_str());
    }

    std::remove(fileName.c_str());
    return 0;
}
