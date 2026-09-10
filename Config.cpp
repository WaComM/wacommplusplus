//
// Created by Raffaele Montella on 08/12/20.
//

#include "Config.hpp"
#include "JulianDate.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>

// for convenience
using json = nlohmann::json;

Config::Config() {
    setDefault();
}

Config::Config(const string &fileName): configFile(fileName) {
    setDefault();

    log4cplus::BasicConfigurator basicConfig;
    basicConfig.configure();
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));

    LOG4CPLUS_DEBUG(logger, "Reading config file:" + fileName);

    if(fileName.substr(fileName.find_last_of(".") + 1) == "json") {
        // The configuration is a json
        loadFromJson(fileName);
    } else {
        // the configuration is a fortran style namelist
        loadFromNamelist(fileName);
    }

}

Config::~Config() {

}


void Config::setDefault() {
    // Is the model dry mode?
    dry = false;

    // Input time step in s
    _data.deltat=3600;

    // Integration time in s
    _data.dti=30;

    // To be clarified
    _data.tau0=86400.0;

    // Probability of particle surviving
    _data.survprob=1.0e-4;

    // Sedimentation velocity
    _data.sv=0;

    // Reduction Coefficient
    _data.crid=1;

    // Diffusion standard deviaion defalt = 3.46
    _data.sigma = 3.46;

    // Shore limit (less deeper than shore limit, a particle is beached
    _data.shoreLimit=.25;

    // File system root where the outputs will be saved ( default current directory )
    ncOutputRoot = "";

    timeStep = 3600;

    // Number of hours to calculate ( default 1)
    nHour = 1;

    // Starting time (default 0)
    startTime = 0;

    // Use restart (default false)
    useRestart = false;

    // Name of the file used for restarts
    restartFile = "WACOMM_rst_.txt";

    // Restart interval
    restartInterval = 3600;

    // Apply mask to the output (default false)
    maskOutput = false;

    // Save history (to be used as restart) default true
    saveHistory = "none";

    // Default file name for the history
    historyRoot = "WACOMM_rst_";

    // Embedded history data into output (default false)
    embeddedHistory = false;

    // Use sources (default true)
    useSources = true;

    // Set the sources file name (defauly empty)
    sourcesFile = "";

    // Use random leap (default true. use false for model testing versus other implementations
    _data.random = true;

    // Generate random sources (default true. use false for model testing versus other imlementations
    _data.randomSources = true;

    // Random seed (default MT19937 seed)
    _data.randomSeed = 5489;

    // Tracking direction (default forward)
    _data.trackingDirection = Config::TRACKING_FORWARD;

    // Backward diffusion is disabled by default
    _data.backwardDiffusion = false;

    // Physical restart checkpoint (not set by default)
    _data.restartCheckpoint = std::numeric_limits<double>::quiet_NaN();

    // Passive particles do not request atmospheric forcing.
    _data.driftModel = 0;
    _data.leewayCoefficientEnsemble = false;
    _data.driftObjectType = static_cast<std::uint16_t>(DriftObjectType::PASSIVE);
    _data.driftSide = static_cast<std::int8_t>(DriftSide::UNDEFINED);
    _data.hasWind = false;
    _data.windU10 = 0;
    _data.windV10 = 0;
    weatherModel="none";
    weatherInputs.clear();
    weatherRegridding="none";
    waveModel="none";
    waveInputs.clear();
    waveRegridding="none";

    // Save processed input files (default false)
    saveInput = false;

    // Root for saved input files.
    ncInputRoot="ocm_";

    // Upper closure mode: 0: constraint; 1: kill; 2: reflection
    _data.upperClosure = Config::CLOSURE_MODE_CONSTRAINT;

    // Lower closure mode: 0: constraint; 1: kill; 2: reflection
    _data.lowerClosure = Config::CLOSURE_MODE_KILL;

    // Horizontal closure mode: 0: constraint; 1: kill; 2: reflection
    _data.horizontalClosure = Config::CLOSURE_MODE_REFLECTION;

    // Define the dictionary
    dictionary["constraint"]=Config::CLOSURE_MODE_CONSTRAINT;
    dictionary["kill"]=Config::CLOSURE_MODE_KILL;
    dictionary["reflection"]=Config::CLOSURE_MODE_REFLECTION;
}

void Config::loadFromNamelist(const string &fileName) {
    setDefault();

    std::ifstream infile(fileName);
    std::string line;
    while (std::getline(infile, line))
    {
        // Trim the line
        line = Utils::trim(line," \t\r");

        // Select the section parser
        if (line == "&io") {
            // s starts with prefix &
            namelistParseIo(infile);
        } else if (line == "&chm") {
            // s starts with prefix &
            namelistParseChm(infile);
        } if (line == "&rst") {
            // s starts with prefix &
            namelistParseRst(infile);
        } if (line == "&hst") {
            // s starts with prefix &
            namelistParseHst(infile);
        }
    }
}

void Config::namelistParseIo(ifstream &infile) {
    std::string line;

    LOG4CPLUS_DEBUG(logger, "Parsing io section");

    // Start the input/output section
    while (std::getline(infile, line))
    {
        // Trim the line
        line = Utils::trim(line, " \t\r");

        // Check for the end of the section
        if (line=="/") { break; }

        vector<string> keyValues;
        Utils::tokenize(line,'=',keyValues);

        if (keyValues.size()==2) {
            string key=Utils::trim(keyValues.at(0)," \t\r");
            if (key == "nc_inputs") {
                // The list of the input files
                vector<string> ncInputs;
                Utils::tokenize(keyValues.at(1),',',ncInputs);

                for(string ncInput: ncInputs) {
                    ncInput=Utils::trim(ncInput," '");
                    this->ncInputs.push_back(ncInput);
                }
            } else if (key == "nc_output_root") {
                this->ncOutputRoot = Utils::trim(keyValues.at(1)," ',");
            } else if (key == "starttime") {
                this->startTime = stoi(keyValues.at(1));
            } else if (key == "nhour") {
                this->nHour = stoi(keyValues.at(1));
            } else if (key == "timestep") {
                this->timeStep = stod(keyValues.at(1));
            }
        }
    }
}

void Config::namelistParseChm(ifstream &infile) {
    std::string line;

    LOG4CPLUS_DEBUG(logger, "Parsing chm section");

    // Start the Chem section
    while (std::getline(infile, line))
    {
        // Trim the line
        line = Utils::trim(line, " \t\r");

        // Check for the end of the section
        if (line=="/") { break; }

        vector<string> keyValues;
        Utils::tokenize(line,'=',keyValues);

        if (keyValues.size()==2) {
            string key=Utils::trim(keyValues.at(0)," \t\r");
            if (key == "tau0") {
                this->_data.tau0 = stod(keyValues.at(1));
            } else if (key == "survprob") {
                this->_data.survprob = stod(keyValues.at(1));
            }
        }
    }
}

void Config::namelistParseRst(ifstream &infile) {
    std::string line;

    LOG4CPLUS_DEBUG(logger, "Parsing rst section");

    // Start the Restart section
    while (std::getline(infile, line))
    {
        // Trim the line
        line = Utils::trim(line, " \t\r");

        // Check for the end of the section
        if (line=="/") { break; }

        vector<string> keyValues;
        Utils::tokenize(line,'=',keyValues);

        if (keyValues.size()==2) {
            string key=Utils::trim(keyValues.at(0)," \t\r");
            if (key == "restart") {
                keyValues.at(1) = Utils::trim(keyValues.at(1), " \t");
                if (keyValues.at(1) == ".true.") {
                    this->useRestart=true;
                } else {
                    this->useRestart=false;
                }
            } else if (key == "restartfile") {
                this->restartFile = Utils::trim(keyValues.at(1)," '\t\r");;
            } else if (key == "interval") {
                this->restartInterval = stod(keyValues.at(1));
            }
        }
    }
}

void Config::namelistParseHst(ifstream &infile) {
    std::string line;

    LOG4CPLUS_DEBUG(logger, "Parsing hst section");

    // Start the History section
    while (std::getline(infile, line))
    {
        // Trim the line
        line = Utils::trim(line, " \t\r");

        // Check for the end of the section
        if (line=="/") { break; }

        vector<string> keyValues;
        Utils::tokenize(line,'=',keyValues);

        if (keyValues.size()==2) {
            string key=Utils::trim(keyValues.at(0)," \t\r");
            if (key == "history") {
                keyValues.at(1) = Utils::trim(keyValues.at(1), " \t");
                if (keyValues.at(1) == ".true.") {
                    this->saveHistory="text";
                }
            } else if (key == "historyfile") {
                this->historyRoot = Utils::trim(keyValues.at(1)," ',");
            }
        }
    }
}

double Config::Dti() const { return _data.dti; }

double Config::Deltat() const { return _data.deltat; }

double Config::Survprob() const {
    return _data.survprob;
}

double Config::ShoreLimit() const {
    return _data.shoreLimit;
}

double Config::Tau0() const {
    return _data.tau0;
}

double Config::SedimentationVelocity() const {
    return _data.sv;
}

std::uint64_t Config::RandomSeed() const { return _data.randomSeed; }

bool Config::Backward() const { return _data.trackingDirection == Config::TRACKING_BACKWARD; }

bool Config::BackwardDiffusion() const { return _data.backwardDiffusion; }

bool Config::Leeway() const { return _data.driftModel==1; }
bool Config::LeewayCoefficientEnsemble() const { return _data.leewayCoefficientEnsemble; }

DriftObjectType Config::DriftObject() const { return static_cast<DriftObjectType>(_data.driftObjectType); }

DriftSide Config::DefaultDriftSide() const { return static_cast<DriftSide>(_data.driftSide); }

void Config::RestartCheckpoint(double value) { _data.restartCheckpoint=value; }

double Config::RestartCheckpoint() const { return _data.restartCheckpoint; }

bool Config::Dry() const {
    return dry;
}

void Config::Dry(bool value) {
    dry=value;
}

double Config::JulianStart() const {
    return julianStart;
}

void Config::JulianStart(double value) {
    julianStart=value;
}

double Config::JulianEnd() const {
    return julianEnd;
}

void Config::JulianEnd(double value) {
    julianEnd=value;
}

int Config::UpperClosure() const {
    return _data.upperClosure;
}

int Config::LowerClosure() const {
    return _data.lowerClosure;
}

int Config::HorizontalClosure() const {
    return _data.horizontalClosure;
}

double Config::ReductionCoefficient() const {
    return _data.crid;
}

string Config::RestartFile() const {
    return restartFile;
}

int Config::RestartInterval() const {
    return restartInterval;
}

bool Config::UseRestart() const {
    return useRestart;
}

string &Config::ConfigFile() {
    return configFile;
}

vector<string> &Config::NcInputs() {
    return ncInputs;
}

void Config::StartTimeIndex(int value) {
    startTime=value;
}

int Config::StartTimeIndex() {
    return startTime;
}

void Config::NumberOfInputs(int value) {
    nHour=value;
}

int Config::NumberOfInputs() {
    return nHour;
}



void Config::Random(bool value) {
    _data.random=value;
}

void Config::RandomSources(bool value) {
    _data.randomSources=value;
}

bool Config::Random() const {
    return _data.random;
}

bool Config::RandomSources() const {
    return _data.randomSources;
}

config_data *Config::dataptr() {
    return &_data;
}

bool Config::UseSources() const {
    return useSources;
}

void Config::UseSources(bool value) {
    useSources = value;
}

void Config::UseRestart(bool value) {
    useRestart = value;
}

void Config::RestartFile(string value) {
    restartFile = value;
}

void Config::RestartInterval(int value) {
    restartInterval = value;
}

string Config::SourcesFile() const {
    return sourcesFile;
}

void Config::SourcesFile(string value) {
    sourcesFile=value;
}

string Config::NcOutputRoot() const {
    return ncOutputRoot;
}

void Config::NcOutputRoot(string value) {
    ncOutputRoot=value;
}

int Config::TimeStep() const {
    return timeStep;
}

void Config::TimeStep(int value) {
    timeStep=value;
}

void  Config::SaveHistory(string value) {
    saveHistory=value;
}

void  Config::HistoryRoot(string value) {
    historyRoot = value;
}

string Config::SaveHistory() const {
    return saveHistory;
}

string Config::HistoryRoot() const {
    return historyRoot;
}

void Config::SaveInput(bool value) {
    saveInput = value;
}

bool Config::SaveInput() const {
    return saveInput;
}

string Config::NcInputRoot() const {
    return ncInputRoot;
}

void Config::NcInputRoot(string value) {
    ncInputRoot = value;
}

bool  Config::EmbeddedHistory() const {
    return embeddedHistory;
}

void Config::EmbeddedHistroy(bool value) {
    embeddedHistory = value;
}

string Config::OceanModel() const {
    return oceanModel;
}

string Config::WeatherModel() const { return weatherModel; }
vector<string>& Config::WeatherInputs() { return weatherInputs; }
string Config::WeatherRegridding() const { return weatherRegridding; }
string Config::WaveModel() const { return waveModel; }
vector<string>& Config::WaveInputs() { return waveInputs; }
string Config::WaveRegridding() const { return waveRegridding; }

void Config::OceanModel(string value) {
    oceanModel = value;
}

string Config::asJson() const {

    Calendar calStart, calEnd;

    JulianDate::fromModJulian(julianStart, calStart);
    JulianDate::fromModJulian(julianEnd, calEnd);

    json simulation = {
            { "name", name },
            { "institution", institution },
            { "url", url},
            { "start", calStart.asNCEPdate()},
            { "end", calEnd.asNCEPdate() }
    };

    json io = {
            { "ocean_model", oceanModel},
            { "base_path", ncBasePath },
            { "nc_inputs", ncInputs },
            { "nc_output_root", ncOutputRoot },
            { "save_history", saveHistory },
            { "save_input", saveInput},
            { "nc_input_root", ncInputRoot}
    };

    json restart = {
            { "active", useRestart },
            { "restart_file", restartFile },
            { "interval", restartInterval }
    };

    json sources = {
            { "active", useSources },
            { "sources_file", sourcesFile }
    };

    json physics = {
            { "tau0", _data.tau0 },
            { "survprob", _data.survprob },
            { "random", _data.random },
            { "random_sources", _data.randomSources },
            { "sv", _data.sv },
            { "dti", _data.dti },
            { "deltat", _data.deltat },
            { "crid", _data.crid },
            { "sigma", _data.sigma },
            { "shore_limit", _data.shoreLimit },
            { "upper_closure", _data.upperClosure == Config::CLOSURE_MODE_CONSTRAINT ? "constraint" :
                                 _data.upperClosure == Config::CLOSURE_MODE_KILL ? "kill" : "reflection" },
            { "lower_closure", _data.lowerClosure == Config::CLOSURE_MODE_CONSTRAINT ? "constraint" :
                                 _data.lowerClosure == Config::CLOSURE_MODE_KILL ? "kill" : "reflection" },
            { "horizontal_closure", _data.horizontalClosure == Config::CLOSURE_MODE_CONSTRAINT ? "constraint" :
                                      _data.horizontalClosure == Config::CLOSURE_MODE_KILL ? "kill" : "reflection" },
            { "random_seed", _data.randomSeed },
    };

    json tracking = {
            { "direction", Backward() ? "backward" : "forward" },
            { "backward_diffusion", BackwardDiffusion() ? "symmetric_stochastic" : "none" }
    };

    json drift = {
            { "model", Leeway() ? "leeway" : "passive" },
            { "coefficient_ensemble", LeewayCoefficientEnsemble() },
            { "object_type", DriftObjectCatalog::name(DriftObject()) },
            { "side", DefaultDriftSide()==DriftSide::LEFT ? "left" :
                      DefaultDriftSide()==DriftSide::RIGHT ? "right" : "undefined" }
    };

    json environment = {
            { "wind", {{"adapter",weatherModel=="WRF" ? "WRF" : (_data.hasWind ? "constant" : "none")},
                        {"u10",_data.windU10},{"v10",_data.windV10},
                        {"nc_inputs",weatherInputs},{"regrid",weatherRegridding}} },
            { "wave", {{"adapter",waveModel},{"nc_inputs",waveInputs},{"regrid",waveRegridding}} }
    };

    json config = {
            { "simulation", simulation},
            { "io", io},
            { "restart", restart},
            { "sources", sources},
            { "physics", physics},
            { "tracking", tracking},
            { "drift", drift},
            { "environment", environment},
    };

    return config.dump(4);
}

void Config::saveAsJson(const string &fileName) {
    // write prettified JSON to another file
    std::ofstream o(fileName);
    o << asJson() << std::endl;
}

void Config::loadFromJson(const string &fileName) {
    setDefault();
    json config;
    std::ifstream i(fileName);
    i >> config;
    if (config.contains("simulation")) {
        json simulation=config["simulation"];
        if (simulation.contains("dry")) { dry = simulation["dry"]; }
        if (simulation.contains("name")) { name = simulation["name"]; }
        if (simulation.contains("institution")) { institution = simulation["institution"]; }
        if (simulation.contains("url")) { url = simulation["url"]; }
        if (simulation.contains("start")) {
            Calendar calStart(simulation["start"]);
            julianStart=JulianDate::toModJulian(calStart);
        }
        if (simulation.contains("end")) {
            Calendar calEnd(simulation["end"]);
            julianEnd=JulianDate::toModJulian(calEnd);
        }
    }
    if (config.contains("io")) {
        json io=config["io"];
        if (io.contains("embedded_history")) { embeddedHistory = io["embedded_history"]; }
        if (io.contains("mask_output")) { maskOutput = io["mask_output"]; }
        if (io.contains("save_history")) { saveHistory = io["save_history"]; }
        if (io.contains("history_root")) {  historyRoot= io["history_root"]; }
        if (io.contains("save_input")) { saveInput = io["save_input"]; }
        if (io.contains("nc_input_root")) { ncInputRoot = io["nc_input_root"]; }
        if (io.contains("base_path")) { ncBasePath = io["base_path"]; }
        if (io.contains("ocean_model")) { oceanModel = io["ocean_model"]; }
        if (io.contains("nc_output_root")) { ncOutputRoot = io["nc_output_root"]; }
        if (io.contains("timestep")) { timeStep = io["timestep"]; }
        if (io.contains("nc_inputs") && io["nc_inputs"].is_array()) {
            for (auto ncInput:io["nc_inputs"]) {
                string file=ncInput;
                this->ncInputs.push_back(ncBasePath+"/"+file);
            }
        }
    }
    if (config.contains("restart")) {
        json restart=config["restart"];
        if (restart.contains("active")) { useRestart = restart["active"]; }
        if (restart.contains("restart_file")) { restartFile = restart["restart_file"]; }
        if (restart.contains("interval")) { restartInterval = restart["interval"]; }
    }
    if (config.contains("sources")) {
        json sources=config["sources"];
        if (sources.contains("active")) { useSources = sources["active"]; }
        if (sources.contains("sources_file")) { sourcesFile = sources["sources_file"]; }
    }
    if (config.contains("physics")) {
        json physics=config["physics"];
        auto closureMode=[this](const json &value) {
            string name=value;
            auto item=dictionary.find(name);
            if (item==dictionary.end()) throw std::runtime_error("Unknown closure mode: " + name);
            return item->second;
        };
        if (physics.contains("tau0")) { _data.tau0 = physics["tau0"]; }
        if (physics.contains("crid")) { _data.crid = physics["crid"]; }
        if (physics.contains("deltat")) { _data.deltat = physics["deltat"]; }
        if (physics.contains("dti")) { _data.dti = physics["dti"]; }
        if (physics.contains("sv")) { _data.sv = physics["sv"]; }
        if (physics.contains("sigma")) { _data.sigma = physics["sigma"]; }
        if (physics.contains("random")) { _data.random = physics["random"]; }
        if (physics.contains("random_sources")) { _data.randomSources = physics["random_sources"]; }
        if (physics.contains("random_seed")) { _data.randomSeed = physics["random_seed"]; }
        if (physics.contains("survprob")) { _data.survprob = physics["survprob"]; }
        if (physics.contains("shore_limit")) { _data.shoreLimit = physics["shore_limit"]; }
        if (physics.contains("upper_closure")) { _data.upperClosure = closureMode(physics["upper_closure"]); }
        if (physics.contains("lower_closure")) { _data.lowerClosure = closureMode(physics["lower_closure"]); }
        if (physics.contains("horizontal_closure")) { _data.horizontalClosure = closureMode(physics["horizontal_closure"]); }
    }
    if (config.contains("tracking")) {
        json tracking=config["tracking"];
        if (tracking.contains("direction")) {
            string direction=tracking["direction"];
            if (direction == "forward") _data.trackingDirection=Config::TRACKING_FORWARD;
            else if (direction == "backward") _data.trackingDirection=Config::TRACKING_BACKWARD;
            else throw std::runtime_error("Unknown tracking direction: " + direction);
        }
        if (tracking.contains("backward_diffusion")) {
            string diffusion=tracking["backward_diffusion"];
            if (diffusion == "none") _data.backwardDiffusion=false;
            else if (diffusion == "symmetric_stochastic") _data.backwardDiffusion=true;
            else throw std::runtime_error("Unknown backward diffusion mode: " + diffusion);
        }
    }
    if (config.contains("drift")) {
        json drift=config["drift"];
        string model=drift.value("model","passive");
        if (model=="passive") _data.driftModel=0;
        else if (model=="leeway") _data.driftModel=1;
        else throw std::runtime_error("Unknown drift.model: " + model);
        if (drift.contains("coefficient_ensemble"))
            _data.leewayCoefficientEnsemble=drift["coefficient_ensemble"];
        if (drift.contains("object_type")) {
            string objectType=drift["object_type"];
            _data.driftObjectType=static_cast<std::uint16_t>(DriftObjectCatalog::type(objectType.c_str()));
        }
        if (drift.contains("side")) {
            string side=drift["side"];
            if (side=="left") _data.driftSide=static_cast<std::int8_t>(DriftSide::LEFT);
            else if (side=="right") _data.driftSide=static_cast<std::int8_t>(DriftSide::RIGHT);
            else if (side=="undefined") _data.driftSide=static_cast<std::int8_t>(DriftSide::UNDEFINED);
            else throw std::runtime_error("Unknown drift.side: " + side);
        }
    }
    if (config.contains("environment")) {
        json environment=config["environment"];
        if (environment.contains("wind")) {
            json wind=environment["wind"];
            string adapter=wind.value("adapter","none");
            if (adapter=="constant") {
                if (!wind.contains("u10") || !wind.contains("v10"))
                    throw std::runtime_error("The constant wind adapter requires environment.wind.u10 and environment.wind.v10");
                _data.windU10=wind["u10"];
                _data.windV10=wind["v10"];
                _data.hasWind=true;
            } else if (adapter=="WRF") {
                weatherModel=adapter; _data.hasWind=true;
                weatherRegridding=wind.value("regrid","none");
                if (weatherRegridding!="none" && weatherRegridding!="bilinear_geographic" &&
                    weatherRegridding!="bilinear_curvilinear_geographic")
                    throw std::runtime_error("Unknown environment.wind.regrid: " + weatherRegridding);
                if (!wind.contains("nc_inputs") || !wind["nc_inputs"].is_array())
                    throw std::runtime_error("WRF wind requires environment.wind.nc_inputs");
                for (auto input:wind["nc_inputs"]) weatherInputs.push_back(input);
            } else if (adapter!="none") throw std::runtime_error("Unknown environment.wind.adapter: " + adapter);
        }
        if (environment.contains("wave")) {
            json wave=environment["wave"]; waveModel=wave.value("adapter","none");
            if (waveModel!="none" && waveModel!="WW3") throw std::runtime_error("Unknown environment.wave.adapter: " + waveModel);
            if (waveModel=="WW3") {
                waveRegridding=wave.value("regrid","none");
                if (waveRegridding!="none" && waveRegridding!="bilinear_geographic" &&
                    waveRegridding!="bilinear_curvilinear_geographic")
                    throw std::runtime_error("Unknown environment.wave.regrid: " + waveRegridding);
                if (!wave.contains("nc_inputs") || !wave["nc_inputs"].is_array())
                    throw std::runtime_error("WW3 wave input requires environment.wave.nc_inputs");
                for (auto input:wave["nc_inputs"]) waveInputs.push_back(input);
            }
        }
    }
    if (_data.driftModel==1 && static_cast<DriftObjectType>(_data.driftObjectType)==DriftObjectType::PASSIVE)
        throw std::runtime_error("drift.model=leeway requires a non-passive drift.object_type");
    if (_data.driftModel==1 && _data.driftSide==static_cast<std::int8_t>(DriftSide::UNDEFINED))
        throw std::runtime_error("drift.model=leeway requires drift.side=left or right");
    if (_data.driftModel==1 && !_data.hasWind)
        throw std::runtime_error("Leeway drift requires 10 m wind. Configure environment.wind.adapter=constant or WRF");
    if (_data.driftModel!=1 && _data.leewayCoefficientEnsemble)
        throw std::runtime_error("drift.coefficient_ensemble requires drift.model=leeway");
    if (_data.hasWind && (!std::isfinite(_data.windU10) || !std::isfinite(_data.windV10)))
        throw std::runtime_error("environment.wind.u10 and environment.wind.v10 must be finite values in m/s");
    if (weatherModel=="WRF" && weatherInputs.size()!=ncInputs.size())
        throw std::runtime_error("WRF and ocean nc_inputs must contain one matching file per forcing window");
    if (waveModel=="WW3" && waveInputs.size()!=ncInputs.size())
        throw std::runtime_error("WW3 and ocean nc_inputs must contain one matching file per forcing window");
    if (!std::isfinite(_data.dti) || _data.dti<=0) {
        throw std::runtime_error("physics.dti must be a finite value greater than zero");
    }
    if (!std::isfinite(_data.deltat) || _data.deltat<=0) {
        throw std::runtime_error("physics.deltat must be a finite value greater than zero");
    }
    if (!std::isfinite(_data.sigma) || _data.sigma<0) {
        throw std::runtime_error("physics.sigma must be a finite value greater than or equal to zero");
    }
    if (!std::isfinite(_data.shoreLimit) || _data.shoreLimit<0) {
        throw std::runtime_error("physics.shore_limit must be a finite value greater than or equal to zero");
    }
}

bool Config::MaskOutput() const {
    return maskOutput;
}

void Config::MaskOutput(bool value) {
    maskOutput = value;
}

double Config::Sigma() const {
    return _data.sigma;
}
