//
// Created by Raffaele Montella on 14/12/20.
//

#include "WacommPlusPlus.hpp"
#include "JulianDate.hpp"
#include "OceanModelAdapterFactory.hpp"
#include "WeatherModelAdapterFactory.hpp"
#include "WaveModelAdapterFactory.hpp"
#include <algorithm>
#include <stdexcept>

#if defined(USE_MPI) || defined(USE_EMPI)
#define OMPI_SKIP_MPICXX
#include <mpi.h>
#endif

#ifdef USE_EMPI
extern "C" {
#include <empi.h>
}
#endif

WacommPlusPlus::~WacommPlusPlus() = default;

WacommPlusPlus::WacommPlusPlus(std::shared_ptr<Config> config): config(config) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));

    // Create the particles
    particles = std::make_shared<Particles>();

    // Create the sources
    sources =  std::make_shared<Sources>();
}



void WacommPlusPlus::run() {
    LOG4CPLUS_DEBUG(logger, "External loop...");

    int world_size = 1, world_rank = 0;
    char mpi_name[128];
    int len, nParticles = 0;

    char bin[1024];
    snprintf(bin,sizeof(bin),"%s","wacomm");

    int idx = 0;
    double p = 17000, t = 0.0;
    double time_average = 0.0;
    double part_average = 0.0;
    double cuda_average = 0.0;

#ifdef USE_MPI
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
#endif

#ifdef USE_EMPI
    MPI_Comm_size(ADM_COMM_WORLD, &world_size);
    MPI_Comm_rank(ADM_COMM_WORLD, &world_rank);

    MPI_Get_processor_name (mpi_name, &len);
    
    if (world_rank == 0) {
        nParticles = -1;
    }

    // get process type
    int proctype;
    ADM_GetSysAttributesInt ("ADM_GLOBAL_PROCESS_TYPE", &proctype);

    // if process is native
    if (proctype == ADM_NATIVE) {
        printf ("Rank(%d/%d): Process native\n", world_rank, world_size);
    // if process is spawned
    } else {
        printf ("Rank(%d/%d): Process spawned\n", world_rank, world_size);
    }

    ADM_GetSysAttributesInt ("ADM_GLOBAL_ITERATION", &idx);
    ADM_RegisterSysAttributesDouble ("ADM_GLOBAL_PARTICLES", &p);
    ADM_RegisterSysAttributesDouble ("ADM_GLOBAL_TIME", &t);

    /* starting monitoring service */
    ADM_MonitoringService (ADM_SERVICE_START);
#endif

    int inputDirection=config->Backward() ? -1 : 1;
    int inputFirst=config->Backward() ? (int)config->NcInputs().size()-1 : 0;
    for (int inputIdx=inputFirst;inputIdx>=0 && inputIdx<config->NcInputs().size();inputIdx+=inputDirection) {
        string &ncInput=config->NcInputs()[inputIdx];

        if (world_rank == 0) {
            LOG4CPLUS_INFO(logger, world_rank << ": Input from Ocean Model: " << ncInput);
        }

        shared_ptr<OceanModelAdapter> oceanModelAdapter=
                OceanModelAdapterFactory::create(config->OceanModel(),ncInput);
        oceanModelAdapter->process();

        shared_ptr<WeatherModelAdapter> weatherModelAdapter;
        if (config->WeatherModel()=="WRF") {
            string &weatherInput=config->WeatherInputs()[inputIdx];
            weatherModelAdapter=WeatherModelAdapterFactory::create(config->WeatherModel(),weatherInput);
            weatherModelAdapter->process();
        }
        shared_ptr<WaveModelAdapter> waveModelAdapter;
        if (config->WaveModel()=="WW3") {
            string &waveInput=config->WaveInputs()[inputIdx];
            waveModelAdapter=WaveModelAdapterFactory::create(config->WaveModel(),waveInput);
            waveModelAdapter->process();
        }

        int adjacentIdx=inputIdx+inputDirection;
        if (adjacentIdx>=0 && adjacentIdx<config->NcInputs().size()) {
            string &adjacentInput=config->NcInputs()[adjacentIdx];
            shared_ptr<OceanModelAdapter> adjacentAdapter=
                    OceanModelAdapterFactory::create(config->OceanModel(),adjacentInput);
            adjacentAdapter->process();
            int boundaryRecord=config->Backward() ? (int)adjacentAdapter->OceanTime().Nx()-1 : 0;
            oceanModelAdapter->appendBoundaryRecord(*adjacentAdapter,boundaryRecord,config->Backward());
            if (weatherModelAdapter) {
                string &adjacentWeatherInput=config->WeatherInputs()[adjacentIdx];
                auto adjacentWeather=WeatherModelAdapterFactory::create(config->WeatherModel(),adjacentWeatherInput);
                adjacentWeather->process();
                int weatherRecord=config->Backward() ? (int)adjacentWeather->Time().Nx()-1 : 0;
                weatherModelAdapter->appendBoundaryRecord(*adjacentWeather,weatherRecord,config->Backward());
            }
            if (waveModelAdapter) {
                string &adjacentWaveInput=config->WaveInputs()[adjacentIdx];
                auto adjacentWave=WaveModelAdapterFactory::create(config->WaveModel(),adjacentWaveInput);
                adjacentWave->process();
                int waveRecord=config->Backward() ? (int)adjacentWave->Time().Nx()-1 : 0;
                waveModelAdapter->appendBoundaryRecord(*adjacentWave,waveRecord,config->Backward());
            }
        }
        if (weatherModelAdapter && config->WeatherRegridding()=="bilinear_geographic")
            weatherModelAdapter->regridBilinearGeographic(oceanModelAdapter->Lon(),oceanModelAdapter->Lat());
        else if (weatherModelAdapter && config->WeatherRegridding()=="bilinear_curvilinear_geographic")
            weatherModelAdapter->regridBilinearCurvilinearGeographic(oceanModelAdapter->Lon(),oceanModelAdapter->Lat());
        if (waveModelAdapter && config->WaveRegridding()=="bilinear_geographic")
            waveModelAdapter->regridBilinearGeographic(oceanModelAdapter->Lon(),oceanModelAdapter->Lat());
        else if (waveModelAdapter && config->WaveRegridding()=="bilinear_curvilinear_geographic")
            waveModelAdapter->regridBilinearCurvilinearGeographic(oceanModelAdapter->Lon(),oceanModelAdapter->Lat());

        Calendar cal;

        // Time in "seconds since 1968-05-23 00:00:00"
        double modJulian=oceanModelAdapter->OceanTime()(0);

        // Convert time in days based
        modJulian=modJulian/86400;

        JulianDate::fromModJulian(modJulian, cal);

        // Check if the rank is 0
        if (world_rank == 0) {

            // Check if it is needed to load the sources
            if (config->UseSources() && sources->empty()) {
                string fileName = config->SourcesFile();
                if (fileName.empty()) {
                    sources->loadFromNamelist(config->ConfigFile());
                } else {
                    if (fileName.substr(fileName.find_last_of('.') + 1) == "json") {
                        // The configuration is a json
                        sources->loadFromJson(fileName, oceanModelAdapter);
                    } else {
                        // the configuration is a fortran style namelist
                        sources->loadFromNamelist(fileName);
                    }
                }
            }

            // Check if the processed input must be saved
            if (config->SaveInput()) {

                // Create the filename
                string inputFilename = config->NcInputRoot() + cal.asNCEPdate() + ".nc";

                // Show a information message
                LOG4CPLUS_INFO(logger,  "Saving processed output: " << inputFilename);

                // Save the processed input
                oceanModelAdapter->saveAsNetCDF(inputFilename);
            }

            // Check if using the restart file (only if it is the first iteration)
            if (idx==0 && config->UseRestart() && !config->RestartFile().empty()) {

                // Get the file name
                string fileName = config->RestartFile();

                // Check if the restart is a NetCDF
                if (fileName.substr(fileName.find_last_of('.') + 1) == "nc") {

                    // The restart is a NetCDF
                    particles->loadFromNetCDF(fileName,config);

                    // Check if the restart is a geojson
                } else if (fileName.substr(fileName.find_last_of('.') + 1) == "json") {

                    // The restart is a json
                    particles->loadFromJson(fileName);
                } else {

                    // the restart is a fortran style text file
                    particles->loadFromTxt(fileName);
                }

            }
        }

        // Check if it is a dry run
        if (!config->Dry()) {
            // Create a new Wacomm object
            Wacomm wacomm(config, oceanModelAdapter, sources, particles,weatherModelAdapter,waveModelAdapter);

            // Run the model
            double cuda=0;

            int status = wacomm.run(t,p,cuda,nParticles,idx);
            
#ifdef USE_EMPI
            // check if process ended after malleable region
            if (status == ADM_ACTIVE) {
                // updata world_rank and size
                MPI_Comm_rank(ADM_COMM_WORLD, &world_rank);
                MPI_Comm_size(ADM_COMM_WORLD, &world_size);

                printf("world_size: %d\n", world_size);
            } else {
                // end the process
                break;
            }
#endif

            time_average += t;
            part_average += p;
            cuda_average += cuda;
        }
        // Go to the next input file
        idx++;
    }

#ifdef USE_EMPI
    /* ending monitoring service */
    ADM_MonitoringService (ADM_SERVICE_STOP);
#endif

    // LOG4CPLUS_INFO(logger,  "Outer Cycle Time (Average): " << time_average / (idx-1));
    // LOG4CPLUS_INFO(logger,  "Outer Cycle Particles/sec (Average): " << part_average / (idx-1));
    // LOG4CPLUS_INFO(logger,  "Inner Cycle (Average) sec: " << cuda_average / (idx-1));
}
