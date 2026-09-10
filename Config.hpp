//
// Created by Raffaele Montella on 08/12/20.
//

#ifndef WACOMMPLUSPLUS_CONFIG_HPP
#define WACOMMPLUSPLUS_CONFIG_HPP

#include <string>
#include <fstream>
#include <cstdint>

#include "Utils.hpp"
#include "DriftModel.hpp"

using namespace std;

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

struct config_data {
    bool random;
    bool randomSources;
    double deltat;
    double dti;
    double survprob;
    double tau0;
    double sv;
    double crid;
    double sigma;
    double shoreLimit;
    int upperClosure;
    int lowerClosure;
    int horizontalClosure;
    std::uint64_t randomSeed;
    int trackingDirection;
    int backwardDiffusion;
    double restartCheckpoint;
    int driftModel;
    bool leewayCoefficientEnsemble;
    double leewayResidualCorrelation;
    bool leewayRandomSide;
    double leewayRightSideProbability;
    double leewayJibeProbabilityHourly;
    double windErrorStdDev;
    std::uint16_t driftObjectType;
    std::int8_t driftSide;
    bool hasWind;
    double windU10;
    double windV10;
};

class Config {
public:
    Config();
    explicit Config(const string &fileName);
    ~Config();

    config_data *dataptr();

    string &ConfigFile();
    vector<string> &NcInputs();

    void Dry(bool value);
    bool Dry() const;

    void JulianStart(double value);
    double JulianStart() const;
    void JulianEnd(double value);
    double JulianEnd() const;

    int UpperClosure() const;
    int LowerClosure() const;
    int HorizontalClosure() const;

    // Random variable
    void Random(bool value);
    bool Random() const;

    // Sources random variable
    void RandomSources(bool value);
    bool RandomSources() const;

    double Sigma() const;

    double ReductionCoefficient() const;
    double Survprob() const;
    double Tau0() const;
    double Dti() const;
    double Deltat() const;
    double ShoreLimit() const;
    double SedimentationVelocity() const;
    std::uint64_t RandomSeed() const;
    bool Backward() const;
    bool BackwardDiffusion() const;
    bool Leeway() const;
    bool LeewayCoefficientEnsemble() const;
    double LeewayResidualCorrelation() const;
    bool LeewayRandomSide() const;
    double LeewayRightSideProbability() const;
    double LeewayJibeProbabilityHourly() const;
    double WindErrorStdDev() const;
    DriftObjectType DriftObject() const;
    DriftSide DefaultDriftSide() const;
    void RestartCheckpoint(double value);
    double RestartCheckpoint() const;

    void SaveHistory(string value);
    string SaveHistory() const;
    string HistoryRoot() const;
    void HistoryRoot(string value);
    int TimeStep() const;
    void TimeStep(int value);

    string NcOutputRoot() const;
    void NcOutputRoot(string value);

    bool EmbeddedHistory() const;
    void EmbeddedHistroy(bool value);

    void SaveInput(bool value);
    bool SaveInput() const;
    string NcInputRoot() const;
    void NcInputRoot(string value);

    void UseRestart(bool value);
    bool UseRestart() const;
    string RestartFile() const;
    void RestartFile(string value);
    int RestartInterval() const;
    void RestartInterval(int value);

    bool MaskOutput() const;
    void MaskOutput(bool value);

    bool UseSources() const;
    void UseSources(bool value);
    string SourcesFile() const;
    void SourcesFile(string value);

    void StartTimeIndex(int value);
    int StartTimeIndex();

    void NumberOfInputs(int value);
    int NumberOfInputs();

    string OceanModel() const;
    void OceanModel(string value);
    string WeatherModel() const;
    vector<string>& WeatherInputs();
    string WeatherRegridding() const;
    string WeatherSourceCrs() const;
    string WaveModel() const;
    vector<string>& WaveInputs();
    string WaveRegridding() const;
    string WaveSourceCrs() const;

    void saveAsJson(const string &fileName);
    string asJson() const;
    void loadFromJson(const string &fileName);
    void loadFromNamelist(const string &fileName);

    static const int CLOSURE_MODE_CONSTRAINT=1;
    static const int CLOSURE_MODE_KILL=2;
    static const int CLOSURE_MODE_REFLECTION=3;
    static const int TRACKING_FORWARD=1;
    static const int TRACKING_BACKWARD=-1;

private:
    log4cplus::Logger logger;

    std::map<std::string, int> dictionary;

    string configFile;

    bool dry;
    bool maskOutput;
    config_data _data;

    string name;
    string institution;
    string url;

    double julianStart;
    double julianEnd;
    double julianRef;

    string oceanModel;
    string weatherModel;
    vector<string> weatherInputs;
    string weatherRegridding;
    string weatherSourceCrs;
    string waveModel;
    vector<string> waveInputs;
    string waveRegridding;
    string waveSourceCrs;
    string ncBasePath;
    vector<string> ncInputs;
    string ncOutputRoot;

    bool saveInput;
    string ncInputRoot;

    double timeStep;
    int nHour;
    int startTime;

    bool useRestart;
    string restartFile;
    double restartInterval;

    bool useSources;
    string sourcesFile;

    string saveHistory;
    string historyRoot;

    bool embeddedHistory;

    void setDefault();

    void namelistParseIo(ifstream &ifstream);
    void namelistParseChm(ifstream &infile);
    void namelistParseRst(ifstream &infile);
    void namelistParseHst(ifstream &infile);


};


#endif //WACOMMPLUSPLUS_CONFIG_HPP
