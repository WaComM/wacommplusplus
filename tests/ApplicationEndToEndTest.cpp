// End-to-end application regression for the complete wacommplusplus executable.
//
// The helper has two modes:
//
//   application_end_to_end_test generate <directory>
//       writes two consecutive native WACOMM forcing windows with a spatially
//       uniform, time-independent horizontal current (u,v), a point source and
//       forward/backward JSON configurations;
//
//   application_end_to_end_test demo <directory>
//       writes a six-hour forward documentation case with NetCDF snapshots;
//
//   application_end_to_end_test verify <directory>
//       compares the JSON particle histories produced by the executable with
//       the analytic solution of dx/dt=u, dy/dt=v and with the forward/backward
//       round trip, and checks a run whose start and release are at 00:30
//       (YYYYMMDDZhhmm) for release time, age, position, and whole-hour output.
//
// The uniform current makes the exact Lagrangian displacement independent of
// the interpolation operator, so the comparison verifies the executable path
// (configuration, adapters, cross-file boundary assembly, source emission,
// particle integration, restart output) rather than a particular stencil.

#include "../OceanModelAdapter.hpp"
#include "../JulianDate.hpp"

#include <nlohmann/json.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

using json = nlohmann::json;

namespace {

class FixtureAdapter: public OceanModelAdapter {
public:
    void process() override {}
};

// Physical parameters of the analytic experiment (SI units).
constexpr double U=0.10;            // eastward current [m s-1]
constexpr double V=0.05;            // northward current [m s-1]
constexpr double WINDOW=3600.0;     // forcing record interval [s]
constexpr int WINDOWS=2;            // number of forcing files (verification case)
constexpr int DEMO_WINDOWS=6;       // number of forcing files (documentation case)
constexpr double LON0=14.0, LAT0=40.7, DLON=0.01, DLAT=0.01;
constexpr int NI=21, NJ=21;
constexpr double SOURCE_LON=14.05, SOURCE_LAT=40.75;
// [m], positive down; h=100 m and sW(0)=0, sW(-1)=-0.5 place it in the upper W interval
// at k = -SOURCE_DEPTH/(h*0.5) (docs/model.md#vertical-coordinate-mapping).
constexpr double SOURCE_DEPTH=10.0, SOURCE_K=-SOURCE_DEPTH/(100*0.5);
constexpr int PARTICLES=4;
constexpr double EARTH_RADIUS=6371000.0; // [m], WaComM++ spherical metric
constexpr double PI=3.14159265358979323846; // portable: MSVC omits M_PI by default

double modJulianSeconds(int hour) {
    Calendar cal(2021,7,1,hour,0,0);
    return JulianDate::toModJulian(cal)*86400.0;
}

void writeWindow(const std::string& fileName, int firstHour) {
    FixtureAdapter fixture;
    fixture.OceanTime().Allocate(2);
    fixture.OceanTime()(0)=modJulianSeconds(firstHour);
    fixture.OceanTime()(1)=modJulianSeconds(firstHour+1);
    fixture.SRho().Allocate(2,-1); fixture.SRho()(-1)=-.75; fixture.SRho()(0)=-.25;
    fixture.SW().Allocate(3,-2); fixture.SW()(-2)=-1; fixture.SW()(-1)=-.5; fixture.SW()(0)=0;
    fixture.Mask().Allocate(NJ,NI); fixture.Mask()=1.0;
    fixture.Lon().Allocate(NJ,NI); fixture.Lat().Allocate(NJ,NI); fixture.H().Allocate(NJ,NI);
    for (int j=0;j<NJ;j++) for (int i=0;i<NI;i++) {
        fixture.Lon()(j,i)=LON0+i*DLON; fixture.Lat()(j,i)=LAT0+j*DLAT; fixture.H()(j,i)=100;
    }
    fixture.Zeta().Allocate(2,NJ,NI); fixture.Zeta()=0.0f;
    fixture.U().Allocate(2,2,NJ,NI,0,-1,0,0); fixture.U()=(float)U;
    fixture.V().Allocate(2,2,NJ,NI,0,-1,0,0); fixture.V()=(float)V;
    fixture.W().Allocate(2,3,NJ,NI,0,-2,0,0); fixture.W()=0.0f;
    fixture.AKT().Allocate(2,3,NJ,NI,0,-2,0,0); fixture.AKT()=0.0f;
    std::string output=fileName;
    fixture.saveAsNetCDF(output);
}

json configuration(const std::string& direction, int windows=WINDOWS) {
    bool backward=direction=="backward";
    json inputs=json::array();
    for (int w=0;w<windows;w++) inputs.push_back("forcing-"+std::to_string(w)+".nc");
    char end[32];
    std::snprintf(end,sizeof(end),"20210701Z%02d00",windows);
    json config={
        {"simulation",{{"dry",false},{"start","20210701Z0000"},{"end",end}}},
        {"io",{{"ocean_model","WACOMM"},{"base_path","."},{"nc_inputs",inputs},
               {"nc_output_root",direction+"-conc-"},{"save_history","text"},
               {"history_root",direction+"-history-"}}},
        {"sources",{{"active",!backward},{"sources_file","sources.json"}}},
        {"restart",{{"active",backward},{"restart_file","forward-history-20210701Z02.txt"}}},
        {"physics",{{"random",false},{"random_sources",false},{"random_seed",5489},
                     {"dti",30},{"deltat",WINDOW}}},
        {"tracking",{{"direction",direction},{"backward_diffusion","none"}}}
    };
    return config;
}

json pointSource(int particles) {
    return {
        {"type","FeatureCollection"},
        {"features",json::array({{
            {"type","Feature"},
            {"properties",{{"id","analytic-uniform-current"},{"start","20210701Z0000"},
                           {"end","20210701Z0000"},
                           {"emission",{{"mode","single_pulse"},{"particles",particles}}}}},
            {"geometry",{{"type","Point"},{"coordinates",{SOURCE_LON,SOURCE_LAT,-SOURCE_DEPTH}}}}
        }})}
    };
}

void generate(const std::string& directory) {
    for (int w=0;w<WINDOWS;w++)
        writeWindow(directory+"/forcing-"+std::to_string(w)+".nc",w);
    std::ofstream(directory+"/sources.json") << pointSource(PARTICLES).dump(2) << "\n";
    for (const std::string direction: {"forward","backward"})
        std::ofstream(directory+"/"+direction+".json") << configuration(direction).dump(2) << "\n";

    // Minute-resolution case: simulation and release at 00:30 inside the first window
    json source=pointSource(PARTICLES);
    source["features"][0]["properties"]["start"]="20210701Z0030";
    source["features"][0]["properties"]["end"]="20210701Z0030";
    std::ofstream(directory+"/sources-minutes.json") << source.dump(2) << "\n";
    json minutes=configuration("forward");
    minutes["simulation"]["start"]="20210701Z0030";
    minutes["sources"]["sources_file"]="sources-minutes.json";
    minutes["io"]["nc_output_root"]="minutes-conc-";
    minutes["io"]["history_root"]="minutes-history-";
    std::ofstream(directory+"/minutes.json") << minutes.dump(2) << "\n";
}

// Documentation case: six hourly windows, NetCDF snapshots, and seeded
// source-position offsets so that parallel trajectories are distinguishable.
void generateDemo(const std::string& directory) {
    for (int w=0;w<DEMO_WINDOWS;w++)
        writeWindow(directory+"/forcing-"+std::to_string(w)+".nc",w);
    std::ofstream(directory+"/sources.json") << pointSource(16).dump(2) << "\n";
    json config=configuration("forward",DEMO_WINDOWS);
    config["io"]["save_history"]="nc";
    config["io"]["history_root"]="demo-history-";
    config["io"]["nc_output_root"]="demo-conc-";
    config["physics"]["random_sources"]=true;
    std::ofstream(directory+"/demo.json") << config.dump(2) << "\n";
}

struct State { double lon, lat, k, health, age, time; };

// Read a C++ style text history: "id i j k health age time object side".
std::map<std::uint64_t,State> particles(const std::string& fileName) {
    std::ifstream input(fileName);
    if (!input) {
        std::cerr << "missing history " << fileName << "\n";
        std::exit(1);
    }
    std::size_t count=0; input >> count;
    std::map<std::uint64_t,State> result;
    for (std::size_t n=0;n<count;n++) {
        std::uint64_t id; double i,j,k,health,age,time; int object,side;
        input >> id >> i >> j >> k >> health >> age >> time >> object >> side;
        assert(input);
        // The fixture grid is rectilinear with node coordinates LON0+i*DLON, LAT0+j*DLAT.
        result[id]={LON0+i*DLON,LAT0+j*DLAT,k,health,age,time};
    }
    return result;
}

void verify(const std::string& directory) {
    auto forward=particles(directory+"/forward-history-20210701Z02.txt");
    auto backward=particles(directory+"/backward-history-20210701Z00.txt");
    assert((int)forward.size()==PARTICLES);
    assert(forward.size()==backward.size());

    // Hourly snapshots at the shared window boundary must exist in both directions
    // (restart.interval=3600 s with modified-Julian round-off in physical time).
    auto forwardMiddle=particles(directory+"/forward-history-20210701Z01.txt");
    auto backwardMiddle=particles(directory+"/backward-history-20210701Z01.txt");
    assert(forwardMiddle.size()==forward.size() && backwardMiddle.size()==forward.size());
    for (auto& [id,state]: forwardMiddle) {
        assert(std::abs(state.age-WINDOW)<1e-3);
        double dx=(state.lon-backwardMiddle.at(id).lon)*PI/180.0*EARTH_RADIUS*std::cos(SOURCE_LAT*PI/180.0);
        double dy=(state.lat-backwardMiddle.at(id).lat)*PI/180.0*EARTH_RADIUS;
        assert(std::hypot(dx,dy)<1e-3);
    }

    // Gridded counts are written once per window (io.timestep=3600 s); each file
    // is stamped with the start of the window's last forcing interval in the
    // tracking direction.
    for (const std::string name: {"forward-conc-20210701Z00.nc","forward-conc-20210701Z01.nc",
                                  "backward-conc-20210701Z02.nc","backward-conc-20210701Z01.nc"}) {
        if (!std::ifstream(directory+"/"+name)) {
            std::cerr << "missing gridded output " << name << "\n";
            std::exit(1);
        }
    }

    // Exact solution of dx/dt=U, dy/dt=V on the sphere for a uniform current:
    // dlat = V T / R and lon advances with the local metric R cos(lat).
    double T=WINDOWS*WINDOW;
    double latEnd=SOURCE_LAT+V*T/EARTH_RADIUS*180.0/PI;
    double phi0=SOURCE_LAT*PI/180.0, phi1=latEnd*PI/180.0;
    double lonEnd=SOURCE_LON+(U/V)*std::log(std::tan(PI/4+phi1/2)/std::tan(PI/4+phi0/2))*180.0/PI;
    double displacement=std::hypot(U,V)*T;

    for (auto& [id,state]: forward) {
        double lon=state.lon, lat=state.lat;
        double dx=(lon-lonEnd)*PI/180.0*EARTH_RADIUS*std::cos(phi1);
        double dy=(lat-latEnd)*PI/180.0*EARTH_RADIUS;
        double error=std::hypot(dx,dy);
        std::cout << "forward id=" << id << " lon=" << lon << " lat=" << lat
                  << " analytic=(" << lonEnd << "," << latEnd << ") error_m=" << error << "\n";
        // 0.1% of the 805 m path: single-precision forcing and the discrete
        // spherical metric, not stochastic spread (random=false).
        assert(error<1e-3*displacement);
        assert(std::abs(state.age-T)<1e-3);
        // w=0 and AKT=0: the release level is preserved in both directions
        assert(std::abs(state.k-SOURCE_K)<1e-12);
        assert(std::abs(backward.at(id).k-SOURCE_K)<1e-12);

        const State& origin=backward.at(id);
        double lonBack=origin.lon, latBack=origin.lat;
        double round=std::hypot((lonBack-SOURCE_LON)*PI/180.0*EARTH_RADIUS*std::cos(phi0),
                                (latBack-SOURCE_LAT)*PI/180.0*EARTH_RADIUS);
        std::cout << "backward id=" << id << " lon=" << lonBack << " lat=" << latBack
                  << " round_trip_m=" << round << "\n";
        assert(round<1e-3);
    }

    // Release at 00:30: output cadence stays on whole hours (origin = start of the hour
    // containing simulation.start), the particle is 1800 s old at 01:00 and 5400 s at 02:00.
    auto minutesMiddle=particles(directory+"/minutes-history-20210701Z01.txt");
    auto minutesEnd=particles(directory+"/minutes-history-20210701Z02.txt");
    assert((int)minutesEnd.size()==PARTICLES && minutesMiddle.size()==minutesEnd.size());
    double releaseTime=modJulianSeconds(0)+1800.0;
    double Tm=WINDOWS*WINDOW-1800.0;
    double latEndM=SOURCE_LAT+V*Tm/EARTH_RADIUS*180.0/PI;
    double phi1m=latEndM*PI/180.0;
    double lonEndM=SOURCE_LON+(U/V)*std::log(std::tan(PI/4+phi1m/2)/std::tan(PI/4+phi0/2))*180.0/PI;
    for (auto& [id,state]: minutesEnd) {
        assert(std::abs(minutesMiddle.at(id).age-1800.0)<1e-3);
        assert(std::abs(state.age-Tm)<1e-3);
        assert(std::abs(state.time-releaseTime)<1e-3);
        double dx=(state.lon-lonEndM)*PI/180.0*EARTH_RADIUS*std::cos(phi1m);
        double dy=(state.lat-latEndM)*PI/180.0*EARTH_RADIUS;
        std::cout << "minutes id=" << id << " age=" << state.age << " error_m=" << std::hypot(dx,dy) << "\n";
        assert(std::hypot(dx,dy)<1e-3*std::hypot(U,V)*Tm);
    }
}

}

int main(int argc, char** argv) {
    if (argc!=3) {
        std::cerr << "usage: " << argv[0] << " generate|demo|verify <directory>\n";
        return 2;
    }
    std::string mode=argv[1];
    if (mode=="generate") generate(argv[2]);
    else if (mode=="demo") generateDemo(argv[2]);
    else if (mode=="verify") verify(argv[2]);
    else return 2;
    return 0;
}
