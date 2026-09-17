//
// Created by Raffaele Montella on 07/12/20.
//

#ifndef WACOMMPLUSPLUS_SOURCE_HPP
#define WACOMMPLUSPLUS_SOURCE_HPP


#include "Particles.hpp"
#include "SourceEmission.hpp"

class Source {
public:
    Source();
    Source(string id, double k, double j, double i, double start, double end, double emissionValue,
           EmissionMode emissionMode, int mode);
    ~Source();

    std::vector<double> emissionTimes(const std::shared_ptr<Config>& config,
                                      double intervalStart, double intervalEnd);
    void emit(const std::shared_ptr<Config>& config, std::shared_ptr<Particles> particles,
              double emissionTime);

    string Id();
    double K();
    double J();
    double I();
    double Start();
    double End();
    int ParticlesPerHour();
    double EmissionValue();
    EmissionMode EmissionSchedule();
    int Mode();

    void Id(string value);
    void K(double value);
    void J(double value);
    void I(double value);
    void Start(double value);
    void End(double value);
    void ParticlesPerHour(int value);
    void EmissionValue(double value);
    void EmissionSchedule(EmissionMode value);
    void Mode(int value);



private:
    log4cplus::Logger logger;

    string id;
    double k;
    double j;
    double i;
    double start;
    double end;
    double emissionValue;
    EmissionMode emissionMode;
    int mode;


};


#endif //WACOMMPLUSPLUS_SOURCE_HPP
