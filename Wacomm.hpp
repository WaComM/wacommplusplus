//
// Created by Raffaele Montella on 12/5/20.
//

#ifndef WACOMMPLUSPLUS_WACOMM_HPP
#define WACOMMPLUSPLUS_WACOMM_HPP

// log4cplus - https://github.com/log4cplus/log4cplus
#include <memory>
#include <cstring>
#include <vector>

#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

#include "Sources.hpp"
#include "Particles.hpp"
#include "Array.h"
#include "OceanModelAdapter.hpp"
#include "WeatherModelAdapter.hpp"
#include "WaveModelAdapter.hpp"
#include "OutputWriter.hpp"

using namespace std;
using namespace Array;

class ROMSAdapter;

// Device copies of the forcing arrays on one GPU
typedef struct WacommVariables{
    double *oceanTimeDevice, *depthIntervalsDevice, *lonRadDevice, *latRadDevice, *maskDevice, *hDevice;
    float *zetaDevice, *uDevice, *vDevice, *wDevice, *aktDevice;
    float *windU10Device, *windV10Device, *stokesUDevice, *stokesVDevice;
    struct config_data *configDevice;
} WacommVariables;

/*
 * Forcing kept on the visible GPUs for the whole run (CUDA builds only). A window is uploaded once,
 * in the first solver interval that uses it. Consecutive windows share one boundary record: the
 * previous window's last record in the tracking direction is bit-identical to this window's first one,
 * because appendBoundaryRecord either copied it from this window or verified it exactly. That record
 * is then copied on the device instead of being uploaded again; every other array is uploaded.
 */
class DeviceForcing {
    public:
        ~DeviceForcing();

        // Device arrays, one set per GPU
        std::vector<WacommVariables> gpus;

        // Records allocated for each time-dependent ocean field, and records of the resident window
        size_t capacity=0, records=0;

        // Grid of the resident window (eta, xi, s_rho, s_w)
        size_t eta=0, xi=0, sRho=0, sW=0;

        // Time (s) of the resident window's last record in the tracking direction
        double boundaryTime=0;

        // Input index whose window begins with the resident boundary record, or -1
        int boundaryInput=-1;

        // Set for each window: the resident boundary record may be reused
        bool reuseBoundary=false;
};

class Wacomm {
    public:

        Wacomm(std::shared_ptr<Config> config,
               std::shared_ptr<OceanModelAdapter> oceanModelAdapter,
               std::shared_ptr<Sources> sources,
               std::shared_ptr<Particles> particles,
               std::shared_ptr<WeatherModelAdapter> weatherModelAdapter=nullptr,
               std::shared_ptr<WaveModelAdapter> waveModelAdapter=nullptr,
               std::shared_ptr<OutputWriter> outputWriter=nullptr,
               std::shared_ptr<DeviceForcing> deviceForcing=nullptr);
        ~Wacomm();

        int run(double &time, double&part, double&cuda, int &nParticles, int &idx);

        // Make the window's forcing resident on every GPU; returns the seconds spent
        double uploadForcing(int num_gpus);

    private:
        friend class WacommPlusPlus;

        log4cplus::Logger logger;

        std::shared_ptr<Config> config;
        std::shared_ptr<OceanModelAdapter> oceanModelAdapter;
        std::shared_ptr<WeatherModelAdapter> weatherModelAdapter;
        std::shared_ptr<WaveModelAdapter> waveModelAdapter;

        std::shared_ptr<Sources> sources;
        std::shared_ptr<Particles> particles;

        // Publishes histories and gridded outputs, synchronously or in background (docs/io.md)
        std::shared_ptr<OutputWriter> outputWriter;

        // Forcing resident on the GPUs across windows
        std::shared_ptr<DeviceForcing> deviceForcing;

        // Static: an asynchronous write may outlive this object
        static void save(const string &fileName, Array4<float> &conc,
                         std::shared_ptr<OceanModelAdapter> oceanModelAdapter, std::shared_ptr<Config> config,
                         bool writeConcentration=true/*, bool add = false*/);
        //void save(const string &filename);

};




#endif //WACOMMPLUSPLUS_WACOMM_HPP
