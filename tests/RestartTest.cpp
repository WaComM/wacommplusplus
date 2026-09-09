#include "../Particles.hpp"
#include "../Provenance.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>

using namespace netCDF;

void createRestart(const string &fileName) {
    NcFile file(fileName,NcFile::replace,NcFile::nc4);
    file.putAtt("wacomm_restart_version","3");
    file.putAtt("tracking_direction","forward");
    file.putAtt("ocean_model","ROMS");
    file.putAtt("checkpoint_time",ncDouble,450.0);
    file.putAtt("random_seed",ncUint64,5489ULL);
    NcDim particles=file.addDim("particles",1),particleTime=file.addDim("particle_time",2);
    NcVar id=file.addVar("id",ncUint64,particles);
    NcVar objectType=file.addVar("object_type",ncUint,particles);
    NcVar driftSide=file.addVar("drift_side",ncInt,particles);
    vector<NcDim> dimensions{particles,particleTime};
    NcVar i=file.addVar("i",ncDouble,dimensions),j=file.addVar("j",ncDouble,dimensions);
    NcVar k=file.addVar("k",ncDouble,dimensions),health=file.addVar("health",ncDouble,dimensions);
    NcVar age=file.addVar("age",ncDouble,dimensions),time=file.addVar("time",ncDouble,dimensions);
    std::uint64_t identity=9007199254740993ULL;
    double iValues[2]={1.25,9},jValues[2]={2.5,9},kValues[2]={-.5,9};
    double healthValues[2]={.75,9},ageValues[2]={450,9},timeValues[2]={0,9};
    id.putVar(&identity); i.putVar(iValues); j.putVar(jValues); k.putVar(kValues);
    unsigned int object=static_cast<unsigned int>(DriftObjectType::PERSON_IN_WATER);
    int side=static_cast<int>(DriftSide::LEFT);
    objectType.putVar(&object); driftSide.putVar(&side);
    health.putVar(healthValues); age.putVar(ageValues); time.putVar(timeValues);
}

int main() {
    const string restartFile="restart-test.nc",configFile="restart-config.json",backwardFile="restart-backward-config.json";
    createRestart(restartFile);
    {
        std::ofstream file(configFile);
        file << R"({"io":{"ocean_model":"ROMS"},"physics":{"random_seed":5489},"tracking":{"direction":"forward"}})";
    }
    auto config=std::make_shared<Config>(configFile);
    {
        const string provenanceFile="provenance-test.nc";
        NcFile file(provenanceFile,NcFile::replace,NcFile::nc4);
        Provenance::writeBuildMetadata(file,*config);
        file.close();
        NcFile check(provenanceFile,NcFile::read);
        string revision,compiler,options,configuration,resolvedConfiguration;
        check.getAtt("wacomm_git_revision").getValues(revision);
        check.getAtt("wacomm_compiler").getValues(compiler);
        check.getAtt("wacomm_cmake_options").getValues(options);
        check.getAtt("wacomm_configuration_file").getValues(configuration);
        check.getAtt("wacomm_configuration").getValues(resolvedConfiguration);
        assert(!revision.empty() && !compiler.empty() && !options.empty());
        assert(configuration==configFile);
        assert(resolvedConfiguration.find("\"random_seed\": 5489")!=string::npos);
        assert(resolvedConfiguration.find("\"direction\": \"forward\"")!=string::npos);
        check.close();
        std::remove(provenanceFile.c_str());
    }
    Particles particles;
    particles.loadFromNetCDF(restartFile,config);
    assert(particles.size()==1);
    assert(particles[0].Id()==9007199254740993ULL);
    assert(particles[0].I()==1.25 && particles[0].J()==2.5 && particles[0].K()==-.5);
    assert(particles[0].DriftObject()==DriftObjectType::PERSON_IN_WATER);
    assert(particles[0].Side()==DriftSide::LEFT);
    assert(config->RestartCheckpoint()==450);
    {
        std::ofstream file(backwardFile);
        file << R"({"io":{"ocean_model":"ROMS"},"physics":{"random_seed":5489},"tracking":{"direction":"backward"}})";
    }
    bool rejected=false;
    try {
        auto backward=std::make_shared<Config>(backwardFile);
        Particles invalid;
        invalid.loadFromNetCDF(restartFile,backward);
    } catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    std::remove(restartFile.c_str()); std::remove(configFile.c_str()); std::remove(backwardFile.c_str());
}
