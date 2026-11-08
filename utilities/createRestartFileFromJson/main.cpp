//
// Create a WaComM++ NetCDF restart from GeoJSON Polygon release areas.
//
// usage: createRestartFileFromJson <configuration.json> <polygons.geojson> <YYYYMMDDZhhmm>
//
// The ocean model grid is read from the first forcing window in the configured tracking
// direction, with the same adapter the solver uses. Particles are sampled with
// PolygonRelease (docs/model.md#polygon-releases), mapped with deplatlon2kji, and written
// with the production restart writer (wacomm_restart_version 3) to restart.restart_file.
// The written restart is read back with the production loader and compared exactly.
//

#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"

#include "../../Config.hpp"
#include "../../JulianDate.hpp"
#include "../../OceanModelAdapterFactory.hpp"
#include "../../Particles.hpp"
#include "../../PolygonRelease.hpp"

#include <netcdf>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

int main(int argc, char **argv) {
    if (argc!=4) {
        std::cerr << "usage: " << argv[0] << " <configuration.json> <polygons.geojson> <YYYYMMDDZhhmm>\n";
        return 2;
    }
    log4cplus::initialize();
    log4cplus::Logger::getRoot().setLogLevel(log4cplus::WARN_LOG_LEVEL);

    try {
        auto config=std::make_shared<Config>(std::string(argv[1]));
        std::string polygonFile=argv[2];
        std::string releaseStamp=argv[3];

        // The configuration that consumes the restart names it
        std::string restartFile=config->RestartFile();
        if (!config->UseRestart() || restartFile.size()<4 || restartFile.substr(restartFile.size()-3)!=".nc")
            throw std::runtime_error("The configuration must set restart.active=true and a NetCDF restart.restart_file");
        if (config->NcInputs().empty())
            throw std::runtime_error("The configuration has no io.nc_inputs");

        // The grid of the first forcing window in the tracking direction
        std::string &input=config->Backward() ? config->NcInputs().back() : config->NcInputs().front();
        auto oceanModelAdapter=OceanModelAdapterFactory::create(config->OceanModel(),input);
        oceanModelAdapter->process();

        // Release time in seconds since 1968-05-23 00:00:00 GMT, at a resolution of one second
        Calendar calRelease(releaseStamp);
        double releaseTime=std::round(JulianDate::toModJulian(calRelease)*86400.0);
        auto &oceanTime=oceanModelAdapter->OceanTime();
        double first=std::round(oceanTime(0)), last=std::round(oceanTime(oceanTime.Nx()-1));
        if (releaseTime<first || releaseTime>last)
            throw std::runtime_error("Release time " + releaseStamp + " is outside the first forcing window " + input);
        // A release on a forcing record takes the record time exactly
        for (int t=0; t<(int)oceanTime.Nx(); t++)
            if (std::round(oceanTime(t))==releaseTime) releaseTime=oceanTime(t);

        auto releases=PolygonRelease::loadFromJson(polygonFile);
        Particles particles;
        PolygonRelease::emit(releases,config,*oceanModelAdapter,releaseTime,particles);

        particles.saveAsNetCDF(restartFile,releaseTime,oceanModelAdapter,config);
        {
            netCDF::NcFile dataFile(restartFile,netCDF::NcFile::write);
            dataFile.putAtt("wacomm_polygon_release_file",polygonFile);
            dataFile.putAtt("wacomm_polygon_release_sampler",POLYGON_RELEASE_SAMPLER);
            dataFile.putAtt("wacomm_polygon_release_time",calRelease.format("%Y-%m-%dT%H:%M:%SZ"));
        }

        // The production loader must reproduce the sampled state exactly
        Particles loaded;
        loaded.loadFromNetCDF(restartFile,config);
        if (loaded.size()!=particles.size())
            throw std::runtime_error("The written restart holds " + std::to_string(loaded.size()) +
                                     " of " + std::to_string(particles.size()) + " particles");
        for (size_t n=0; n<particles.size(); n++) {
            Particle &a=particles.at(n), &b=loaded.at(n);
            if (a.Id()!=b.Id() || a.K()!=b.K() || a.J()!=b.J() || a.I()!=b.I() ||
                a.Time()!=b.Time() || a.Age()!=b.Age() || a.Health()!=b.Health() ||
                a.DriftObject()!=b.DriftObject() || a.Side()!=b.Side())
                throw std::runtime_error("The written restart does not reproduce particle " + std::to_string(a.Id()));
        }

        std::cout << "Wrote " << particles.size() << " particles from " << releases.size()
                  << " polygon release(s) at " << calRelease.format("%Y-%m-%dT%H:%M:%SZ")
                  << " (random_seed=" << config->RandomSeed() << ") to " << restartFile << "\n";
    } catch (const std::exception &e) {
        std::cerr << "createRestartFileFromJson: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
