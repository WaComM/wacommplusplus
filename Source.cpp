//
// Created by Raffaele Montella on 07/12/20.
//

#include "Source.hpp"

Source::Source(string id, double k, double j, double i, double start, double end, double emissionValue,
               EmissionMode emissionMode, int mode):
    id(id),k(k),j(j),i(i),start(start),end(end),emissionValue(emissionValue),emissionMode(emissionMode),mode(mode) {

    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));
}

Source::~Source() = default;

std::vector<double> Source::emissionTimes(const std::shared_ptr<Config>& config,
                                          double intervalStart, double intervalEnd) {
    if (mode!=1) return {};
    double sourceStart=start<0 ? config->JulianStart()*86400.0 : start*86400.0;
    double sourceEnd=end<0 ? -1 : end*86400.0;
    return sourceEmissionTimes(emissionMode,emissionValue,sourceStart,sourceEnd,
                               intervalStart,intervalEnd);
}

void Source::emit(const std::shared_ptr<Config>& config, std::shared_ptr<Particles> particles,
                  double emissionTime) {
    std::uint64_t id=particles->size()==0 ? 0 : particles->at(particles->size()-1).Id()+1;
    double kk = k;
    double jj = j;
    double ii = i;

    if (config->RandomSources()) {
        kk = k + sourcePositionOffset(config->RandomSeed(),id,0);
        jj = j + sourcePositionOffset(config->RandomSeed(),id,1);
        ii = i + sourcePositionOffset(config->RandomSeed(),id,2);
    }

    Particle particle(id, kk, jj, ii, emissionTime);
    DriftSide side=config->DefaultDriftSide();
    if (config->LeewayRandomSide())
        side=sampleDriftSide(config->RandomSeed(),id,config->LeewayRightSideProbability());
    particle.Drift(config->DriftObject(),side);
    particles->push_back(particle);
}

Source::Source(): id(""),k(0),j(0),i(0),start(-1),end(-1),emissionValue(100),
                  emissionMode(EmissionMode::FORCING_INTERVAL_BATCH),mode(1) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));
}

void Source::Id(string value) { id = value; }
void Source::K(double value) { k = value; }
void Source::J(double value) { j = value; }
void Source::I(double value) { i = value; }
void Source::Start(double value) { start = value; }
void Source::End(double value) { end = value; }
void Source::ParticlesPerHour(int value) { emissionValue = value; emissionMode=EmissionMode::FORCING_INTERVAL_BATCH; }
void Source::EmissionValue(double value) { emissionValue = value; }
void Source::EmissionSchedule(EmissionMode value) { emissionMode = value; }
void Source::Mode(int value) { mode = value; }

string Source::Id() { return id; }

double Source::K() { return k; }
double Source::J() { return j; }
double Source::I() { return i; }

double Source::Start() { return start; }
double Source::End() { return end; }

int Source::ParticlesPerHour() { return (int)emissionValue; }
double Source::EmissionValue() { return emissionValue; }
EmissionMode Source::EmissionSchedule() { return emissionMode; }

int Source::Mode() { return mode; }
