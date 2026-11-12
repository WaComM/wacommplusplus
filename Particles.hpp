//
// Created by Raffaele Montella on 12/5/20.
//

#ifndef WACOMMPLUSPLUS_PARTICLES_HPP
#define WACOMMPLUSPLUS_PARTICLES_HPP

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

#include "Particle.hpp"

using namespace std;

class Particles : private vector<Particle> {

public:
    Particles();
    ~Particles();

    using vector::push_back;
    using vector::operator[];
    using vector::size;
    using vector::at;
    using vector::empty;
    using vector::begin;
    using vector::end;
    using vector::erase;

    void loadFromTxt(const string& fileName);
    void loadFromJson(const string &fileName);
    void loadFromNetCDF(const string &fileName, std::shared_ptr<Config> config);

    void saveAsTxt(const string& fileName);
    void saveAsJson(const string &fileName, double particleTime, std::shared_ptr<OceanModelAdapter> oceanModelAdapter);
    void saveAsNetCDF(const string &fileName, double particleTime, std::shared_ptr<OceanModelAdapter> oceanModelAdapter,
                      std::shared_ptr<Config> config);

    // NetCDF history records: particles inside the interpolation domain, in particle order
    struct NetCDFHistory {
        vector<std::uint64_t> id;
        vector<double> lat, lon, dep, j, i, k, health, age, time;
        vector<unsigned int> driftObjectType;
        vector<int> driftSide;
    };
    NetCDFHistory netcdfRecords(std::shared_ptr<OceanModelAdapter> oceanModelAdapter) const;

    // Schema and metadata of a NetCDF history of count records; shared by serial and collective writers
    static void defineNetCDF(netCDF::NcFile &dataFile, size_t count, double particleTime, std::shared_ptr<Config> config);
    static void putNetCDF(netCDF::NcFile &dataFile, const NetCDFHistory &records);

    // Text history layout; shared by serial and collective writers
    static void writeTxtHeader(std::ostream &out, size_t count);
    static void writeTxtRecord(std::ostream &out, const Particle &particle);

private:
    log4cplus::Logger logger;
};


#endif //WACOMMPLUSPLUS_PARTICLES_HPP
