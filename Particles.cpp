//
// Created by Raffaele Montella on 12/5/20.
//

#include "Particles.hpp"
#include "Provenance.hpp"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <limits>
#include <stdexcept>

// for convenience
using json = nlohmann::json;

Particles::Particles() {

    // Logger configuration
    log4cplus::BasicConfigurator basicConfig;
    basicConfig.configure();
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));

    LOG4CPLUS_DEBUG(logger, "Empty Particles");
}

void Particles::writeTxtHeader(std::ostream &out, size_t count)
{
    out << "\t" << (int)count << endl;
}

void Particles::writeTxtRecord(std::ostream &out, const Particle &particle)
{
    // Round-trip precision: physical times are O(1e9) seconds
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << particle.Id() << " " << particle.I() << " "  << particle.J() << " " <<  particle.K() << " " << particle.Health() << " " <<  particle.Age() << " " << particle.Time() << " "
        << static_cast<int>(particle.DriftObject()) << " " << static_cast<int>(particle.Side()) << endl;
}

void Particles::saveAsTxt(const string& fileName)
{
    LOG4CPLUS_DEBUG(logger, "Saving restart file:"+fileName);
    std::ofstream outfile(fileName);

    int count = size();
    writeTxtHeader(outfile, count);
    for(int idx=0; idx<count; idx++) {
        writeTxtRecord(outfile, at(idx));
    }

    // A restart that cannot be written completely must not be published
    outfile.close();
    if (!outfile) throw std::runtime_error("Unable to write the text history " + fileName);
}

void Particles::saveAsJson(const string &fileName, double particleTime, std::shared_ptr<OceanModelAdapter> oceanModelAdapter)
{
    json features = json::array();

    for (auto particle:*this) {

        if (particle.J() < 0 || particle.I() < 0 || particle.K() > 0) {
            continue;
        }

        json coordinates = json::array();
        double dep, lat, lon;
        oceanModelAdapter->kji2deplatlon(particle.K(), particle.J(), particle.I(), dep, lat, lon);

        if (dep == 1e37 || lat == 1e37 || lon == 1e37) {
            continue;
        }
        coordinates.push_back(lon);
        coordinates.push_back(lat);

        json properties = {
                { "id", particle.Id()},
                { "k", particle.K()},
                { "j", particle.J()},
                { "i", particle.I()},
                { "health", particle.Health()},
                { "age", particle.Age()},
                { "time", particle.Time()},
                { "depth", dep},
                { "object_type", DriftObjectCatalog::name(particle.DriftObject())},
                { "drift_side", static_cast<int>(particle.Side())}
        };

        json geometry = {
                { "type", "Point"},
                { "coordinates", coordinates }
        };

        json feature = {
                {"type", "Feature"},
                {"properties", properties},
                {"geometry", geometry}
        };

        features.push_back(feature);
    }

    json featureCollection = {
            {"type", "FeatureCollection"},
            {"features", features}
    };

    // write prettified JSON to another file
    std::ofstream o(fileName);
    o << std::setw(4) << featureCollection << std::endl;
    o.close();
    if (!o) throw std::runtime_error("Unable to write the GeoJSON history " + fileName);
}

Particles::NetCDFHistory Particles::netcdfRecords(std::shared_ptr<OceanModelAdapter> oceanModelAdapter) const
{
    NetCDFHistory records;

    // Only particles inside the interpolation domain are stored (the order is preserved)
    for (const auto& particle:*this) {

        if (particle.J() < 0 || particle.I() < 0 || particle.K() > 0) {
            continue;
        }

        double depth, latitude, longitude;
        oceanModelAdapter->kji2deplatlon(particle.K(), particle.J(), particle.I(), depth, latitude, longitude);

        if (depth == 1e37 || latitude == 1e37 || longitude == 1e37) {
            continue;
        }

        records.id.push_back(particle.Id());
        records.lat.push_back(latitude);
        records.lon.push_back(longitude);
        records.dep.push_back(depth);
        records.j.push_back(particle.J());
        records.i.push_back(particle.I());
        records.k.push_back(particle.K());
        records.health.push_back(particle.Health());
        records.age.push_back(particle.Age());
        records.time.push_back(particle.Time());
        records.driftObjectType.push_back(static_cast<unsigned int>(particle.DriftObject()));
        records.driftSide.push_back(static_cast<int>(particle.Side()));
    }
    return records;
}

void Particles::defineNetCDF(netCDF::NcFile &dataFile, size_t count, double particleTime, std::shared_ptr<Config> config)
{
    size_t particle_time=1;

    Provenance::writeBuildMetadata(dataFile,*config);

    dataFile.putAtt("wacomm_restart_version","3");
    dataFile.putAtt("tracking_direction",config->Backward() ? "backward" : "forward");
    dataFile.putAtt("ocean_model",config->OceanModel());
    dataFile.putAtt("checkpoint_time",ncDouble,particleTime);
    dataFile.putAtt("random_seed",ncUint64,static_cast<unsigned long long>(config->RandomSeed()));

    NcDim particlesDim = dataFile.addDim("particles", count);
    NcDim particleTimeDim = dataFile.addDim("particle_time", particle_time);

    NcVar particleTimeVar = dataFile.addVar("particle_time", ncDouble, particleTimeDim);
    particleTimeVar.putAtt("long_name","time since initialization");
    particleTimeVar.putAtt("units","seconds since 1968-05-23 00:00:00 GMT");
    particleTimeVar.putAtt("calendar","gregorian");
    particleTimeVar.putAtt("field","time, scalar, series");
    particleTimeVar.putAtt("_CoordinateAxisType","Time");
    particleTimeVar.putVar(&particleTime);


    NcVar idVar = dataFile.addVar("id", ncUint64, particlesDim);
    idVar.putAtt("long_name","id");
    idVar.putAtt("cf_role", "trajectory_id");

    NcVar objectTypeVar = dataFile.addVar("object_type", ncUint, particlesDim);
    objectTypeVar.putAtt("long_name","drift object type identifier");

    NcVar driftSideVar = dataFile.addVar("drift_side", ncInt, particlesDim);
    driftSideVar.putAtt("long_name","crosswind orientation: -1 left, 0 undefined, 1 right");

    vector<NcDim> particlesParticleTimeDims;
    particlesParticleTimeDims.push_back(particlesDim);
    particlesParticleTimeDims.push_back(particleTimeDim);

    NcVar latVar = dataFile.addVar("lat", ncDouble, particlesParticleTimeDims);
    latVar.putAtt("long_name","latitude of particle");
    latVar.putAtt("unit","degree_north");
    latVar.putAtt("standard_name","latitude");
    //latVar.putAtt("field","lat, scalar");
    //latVar.putAtt("_coordinateaxistype","lat");

    NcVar lonVar = dataFile.addVar("lon", ncDouble, particlesParticleTimeDims);
    lonVar.putAtt("long_name","longitude of particle");
    lonVar.putAtt("unit","degree_east");
    lonVar.putAtt("standard_name","longitude");
    //lonVar.putAtt("field","lon, scalar");
    //lonVar.putAtt("_coordinateaxistype","lon");

    NcVar depVar = dataFile.addVar("depth", ncDouble, particlesParticleTimeDims);
    depVar.putAtt("long_name","particle depth below sea surface");
    depVar.putAtt("unit","meters");
    depVar.putAtt("standard_name","depth");
    depVar.putAtt("positive","up");
    depVar.putAtt("axis","Z");
    //depVar.putAtt("_coordinateaxistype","depth");

    NcVar KVar = dataFile.addVar("k", ncDouble, particlesParticleTimeDims);
    KVar.putAtt("long_name","depth fractional index");

    NcVar JVar = dataFile.addVar("j", ncDouble, particlesParticleTimeDims);
    JVar.putAtt("long_name","latitude fractional index");

    NcVar IVar = dataFile.addVar("i", ncDouble, particlesParticleTimeDims);
    IVar.putAtt("long_name","longitude fractional index");

    NcVar HealthVar = dataFile.addVar("health", ncDouble, particlesParticleTimeDims);
    HealthVar.putAtt("long_name","health of the particle");
    HealthVar.putAtt("units","1e-9") ;
    HealthVar.putAtt("coordinates","ocean_time lon lat depth") ;

    NcVar AgeVar = dataFile.addVar("age", ncDouble, particlesParticleTimeDims);
    AgeVar.putAtt("long_name","age of the particle");
    AgeVar.putAtt("units","seconds since emission");
    AgeVar.putAtt("coordinates","ocean_time lon lat depth") ;

    NcVar TimeVar = dataFile.addVar("time", ncDouble, particlesParticleTimeDims);
    TimeVar.putAtt("long_name","emission time of the particle");
    TimeVar.putAtt("units","seconds since 1968-05-23 00:00:00 GMT");
    TimeVar.putAtt("calendar","gregorian");
    TimeVar.putAtt("coordinates","ocean_time lon lat depth") ;
    //TimeVar.putAtt("field","time, scalar, series");
    //TimeVar.putAtt("_CoordinateAxisType","Time");
}

void Particles::putNetCDF(netCDF::NcFile &dataFile, const NetCDFHistory &records)
{
    // An empty history keeps zero-length variables
    if (records.id.empty()) return;

    dataFile.getVar("id").putVar(records.id.data());
    dataFile.getVar("object_type").putVar(records.driftObjectType.data());
    dataFile.getVar("drift_side").putVar(records.driftSide.data());
    dataFile.getVar("lat").putVar(records.lat.data());
    dataFile.getVar("lon").putVar(records.lon.data());
    dataFile.getVar("depth").putVar(records.dep.data());
    dataFile.getVar("k").putVar(records.k.data());
    dataFile.getVar("j").putVar(records.j.data());
    dataFile.getVar("i").putVar(records.i.data());
    dataFile.getVar("health").putVar(records.health.data());
    dataFile.getVar("age").putVar(records.age.data());
    dataFile.getVar("time").putVar(records.time.data());
}

void Particles::saveAsNetCDF(const string &fileName, double particleTime, std::shared_ptr<OceanModelAdapter> oceanModelAdapter,
                             std::shared_ptr<Config> config)
{
    // https://github.com/NOAA-ORR-ERD/nc_particles
    // https://cfconventions.org/Data/cf-conventions/cf-conventions-1.7/build/aphs04.html

    LOG4CPLUS_INFO(logger,"Preparing NetCDF...");

    NetCDFHistory records=netcdfRecords(oceanModelAdapter);

    LOG4CPLUS_INFO(logger,"Saving NetCDF in: " << fileName);

    // Open the file for read access
    netCDF::NcFile dataFile(fileName, NcFile::replace,NcFile::nc4);

    defineNetCDF(dataFile, records.id.size(), particleTime, config);
    putNetCDF(dataFile, records);

/*
    NcDim oceanTimeDim = dataFile.addDim("ocean_time", ocean_time);
    NcVar oceanTimeVar = dataFile.addVar("ocean_time", ncDouble, oceanTimeDim);
    oceanTimeVar.putAtt("long_name","time since initialization");
    oceanTimeVar.putAtt("units","seconds since 1968-05-23 00:00:00 GMT");
    oceanTimeVar.putAtt("calendar","gregorian");
    oceanTimeVar.putAtt("field","time, scalar, series");
    oceanTimeVar.putAtt("_CoordinateAxisType","Time");
    oceanTimeVar.putVar(oceanModelAdapter->OceanTime()());

    NcDim latitudeDim = dataFile.addDim("latitude", oceanModelAdapter->Latitude().Nx());
    NcVar laitudeVar = dataFile.addVar("latitude", ncDouble, latitudeDim);
    laitudeVar.putAtt("long_name","latitude o");
    laitudeVar.putAtt("unit","degree_north");
    laitudeVar.putAtt("standard_name","latitude");
    laitudeVar.putAtt("field","latitude, scalar");
    laitudeVar.putAtt("_coordinateaxistype","lat");
    laitudeVar.putVar(oceanModelAdapter->Latitude()());

    NcDim longitudeDim = dataFile.addDim("longitude", oceanModelAdapter->Longitude().Nx());
    NcVar longitudeVar = dataFile.addVar("longitude", ncDouble, longitudeDim);
    longitudeVar.putAtt("long_name","longitude o");
    longitudeVar.putAtt("unit","degree_north");
    longitudeVar.putAtt("standard_name","longitude");
    longitudeVar.putAtt("field","latitude, scalar");
    longitudeVar.putAtt("_coordinateaxistype","lon");
    longitudeVar.putVar(oceanModelAdapter->Longitude()());

    vector<NcDim> oceanTimeDepthLatitudeLongitudeDims;
    oceanTimeDepthLatitudeLongitudeDims.push_back(oceanTimeDim);
    oceanTimeDepthLatitudeLongitudeDims.push_back(depthDim);
    oceanTimeDepthLatitudeLongitudeDims.push_back(latitudeDim);
    oceanTimeDepthLatitudeLongitudeDims.push_back(longitudeDim);

    NcVar concVar = dataFile.addVar("conc", ncFloat, oceanTimeDepthLatitudeLongitudeDims);
    concVar.putAtt("long_name","concentration_of_suspended_matter_in_sea_water");
    concVar.putAtt("units","1");
    concVar.putAtt("coordinates","longitude latitude depth ocean_time");
    concVar.putAtt("field","");
    concVar.putAtt("time","ocean_time");
    concVar.putAtt("_FillValue",ncFloat, 9.99999993e+36);
    concVar.putVar(conc());
    */
}

void Particles::loadFromTxt(const string &fileName) {
    std::ifstream infile(fileName);

    unsigned long count=0;
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        if (count > 0)
        {
            std::uint64_t id=count;
            double k, j, i, health, tpart, emitOceanTime=-1;
            //if (!(iss >> i >> j >> k >> health >> tpart )) { break; } // error

            vector<string> tokens{istream_iterator<string>{iss},
                                  istream_iterator<string>{}};

            // Check if the text file has been produced by fortran WaComM
            if (tokens.size()==5) {
                // Fortran style restart text file
                i=atof(tokens[0].c_str());
                j=atof(tokens[1].c_str());
                k=atof(tokens[2].c_str());
                health=atof(tokens[3].c_str());
                tpart=atof(tokens[4].c_str());
            } else if (tokens.size()>=7) {
                // C++ style restart text file
                id=std::stoull(tokens[0]);
                i=atof(tokens[1].c_str());
                j=atof(tokens[2].c_str());
                k=atof(tokens[3].c_str());
                health=atof(tokens[4].c_str());
                tpart=atof(tokens[5].c_str());
                emitOceanTime=atof(tokens[6].c_str());
            } else {
                throw std::runtime_error("Invalid particle restart record");
            }
            Particle particle(id, k, j, i, health, tpart, emitOceanTime);
            if (tokens.size()>=9) particle.Drift(static_cast<DriftObjectType>(atoi(tokens[7].c_str())),
                                                 static_cast<DriftSide>(atoi(tokens[8].c_str())));
            push_back(particle);
        }
        count++;
    }
}

void Particles::loadFromJson(const string &fileName) {
    // The GeoJSON history omits inactive and out-of-domain particles and the
    // restart metadata, so it cannot reproduce the continuous integration.
    throw std::runtime_error("GeoJSON particle histories are not restart files: " + fileName +
                             " (use save_history=nc or text)");
}

void Particles::loadFromNetCDF(const string &fileName, std::shared_ptr<Config> config) {
    LOG4CPLUS_INFO(logger,"Reading restart file: " << fileName);
    std::ifstream infile(fileName);
    
    // Open the file for read access
    netCDF::NcFile dataFile(fileName, NcFile::read);

    NcGroupAtt versionAtt=dataFile.getAtt("wacomm_restart_version");
    if (!versionAtt.isNull()) {
        string restartVersion;
        versionAtt.getValues(restartVersion);
        if (restartVersion != "2" && restartVersion != "3") {
            throw std::runtime_error("Unsupported restart version: " + restartVersion);
        }
        if (restartVersion=="2" && config->Leeway())
            throw std::runtime_error("Restart version 2 does not contain drift object state required by drift.model=leeway");
        string restartDirection;
        dataFile.getAtt("tracking_direction").getValues(restartDirection);
        string configuredDirection=config->Backward() ? "backward" : "forward";
        if (restartDirection != configuredDirection) {
            throw std::runtime_error("Restart tracking direction is " + restartDirection +
                                     " but configuration direction is " + configuredDirection);
        }
        NcGroupAtt checkpointAtt=dataFile.getAtt("checkpoint_time");
        if (!checkpointAtt.isNull()) {
            double checkpointTime;
            checkpointAtt.getValues(&checkpointTime);
            config->RestartCheckpoint(checkpointTime);
        }
        NcGroupAtt seedAtt=dataFile.getAtt("random_seed");
        if (!seedAtt.isNull()) {
            std::uint64_t restartSeed;
            seedAtt.getValues(&restartSeed);
            if (restartSeed != config->RandomSeed()) {
                throw std::runtime_error("Restart random seed does not match the configured random seed");
            }
        }
        // A restart written under another random-key scheme would continue with a different
        // realization; restarts that predate the attribute carry no scheme and are accepted.
        NcGroupAtt schemeAtt=dataFile.getAtt("wacomm_random_key_scheme");
        if (!schemeAtt.isNull()) {
            string restartScheme;
            schemeAtt.getValues(restartScheme);
            if (restartScheme != NumericalHelpers::RANDOM_KEY_SCHEME) {
                throw std::runtime_error("Restart random-key scheme is " + restartScheme +
                                         " but this build uses " + NumericalHelpers::RANDOM_KEY_SCHEME);
            }
        }
        NcGroupAtt modelAtt=dataFile.getAtt("ocean_model");
        if (!modelAtt.isNull()) {
            string restartModel;
            modelAtt.getValues(restartModel);
            if (restartModel != config->OceanModel()) {
                throw std::runtime_error("Restart ocean model is " + restartModel +
                                         " but configuration ocean model is " + config->OceanModel());
            }
        }
    }

    // Retrieve the variable named "id"
    NcVar varId = dataFile.getVar("id");
    size_t size = varId.getDim(0).getSize();
    Array1<std::uint64_t> Id(size);
    varId.getVar(Id());

    // Retrieve the variable named "i"
    NcVar varI = dataFile.getVar("i");
    Array1<double> I(size);
    varI.getVar({0,0},{size,1},I());

    // Retrieve the variable named "j"
    NcVar varJ = dataFile.getVar("j");
    Array1<double> J(size);
    varJ.getVar({0,0},{size,1},J());

    // Retrieve the variable named "k"
    NcVar varK = dataFile.getVar("k");
    Array1<double> K(size);
    varK.getVar({0,0},{size,1},K());

    // Retrieve the variable named "health"
    NcVar varHealth = dataFile.getVar("health");
    Array1<double> health(size);
    varHealth.getVar({0,0},{size,1},health());

    // Retrieve the variable named "age"
    NcVar varAge = dataFile.getVar("age");
    Array1<double> age(size);
    varAge.getVar({0,0},{size,1},age());

    // Retrieve the variable named "time"
    NcVar varTime = dataFile.getVar("time");
    Array1<double> time(size);
    varTime.getVar({0,0},{size,1},time());

    Array1<unsigned int> driftObjectType(size); driftObjectType=static_cast<unsigned int>(0);
    Array1<int> driftSide(size); driftSide=0;
    NcVar varObjectType=dataFile.getVar("object_type");
    NcVar varDriftSide=dataFile.getVar("drift_side");
    if (!varObjectType.isNull()) varObjectType.getVar(driftObjectType());
    if (!varDriftSide.isNull()) varDriftSide.getVar(driftSide());

    for (int i=0; i<size; i++) {
        Particle particle(Id[i], K[i], J[i], I[i], health[i], age[i], time[i]);
        particle.Drift(static_cast<DriftObjectType>(driftObjectType[i]),static_cast<DriftSide>(driftSide[i]));
        push_back(particle);
    }
}

Particles::~Particles() = default;
