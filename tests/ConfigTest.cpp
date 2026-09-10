#include "../Config.hpp"
#include "../JulianDate.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <stdexcept>

int main() {
    Calendar compact("2020010100"),zulu("20200101Z00");
    assert(compact.get(Calendar::YEAR)==2020 && compact.get(Calendar::MONTH)==0);
    assert(zulu.get(Calendar::DAY_OF_MONTH)==1 && zulu.get(Calendar::HOUR_OF_DAY)==0);
    bool invalidCalendarRejected=false;
    try { Calendar invalidCalendar("2020130100"); }
    catch (const std::invalid_argument &) { invalidCalendarRejected=true; }
    assert(invalidCalendarRejected);
    const string input="config-test-input.json",saved="config-test-saved.json",invalid="config-test-invalid.json";
    const string invalidStep="config-test-invalid-step.json";
    {
        std::ofstream file(input);
        file << R"({
          "simulation":{"start":"2020010100","end":"2020010200"},
          "io":{"ocean_model":"ROMS","base_path":".","nc_inputs":["forcing.nc"]},
          "restart":{"active":true,"restart_file":"checkpoint.nc","interval":1800},
          "physics":{"random":true,"random_seed":9007199254740993,"sigma":2.5,"shore_limit":0.4,
                     "upper_closure":"reflection","lower_closure":"constraint","horizontal_closure":"kill"},
          "tracking":{"direction":"backward","backward_diffusion":"symmetric_stochastic"}
        })";
    }
    Config config(input);
    assert(config.RandomSeed()==9007199254740993ULL);
    assert(config.Sigma()==2.5 && config.ShoreLimit()==.4);
    assert(config.UpperClosure()==Config::CLOSURE_MODE_REFLECTION);
    assert(config.LowerClosure()==Config::CLOSURE_MODE_CONSTRAINT);
    assert(config.HorizontalClosure()==Config::CLOSURE_MODE_KILL);
    assert(config.RestartInterval()==1800 && config.Backward() && config.BackwardDiffusion());
    config.saveAsJson(saved);
    Config restored(saved);
    assert(restored.RandomSeed()==config.RandomSeed());
    assert(restored.Sigma()==config.Sigma() && restored.ShoreLimit()==config.ShoreLimit());
    assert(restored.UpperClosure()==config.UpperClosure());
    assert(restored.LowerClosure()==config.LowerClosure());
    assert(restored.HorizontalClosure()==config.HorizontalClosure());
    assert(restored.RestartInterval()==config.RestartInterval());
    const string leeway="config-test-leeway.json";
    {
        std::ofstream file(leeway);
        file << R"({"drift":{"model":"leeway","object_type":"KAYAK_WITH_PERSON","side":"right","coefficient_ensemble":true,"residual_correlation":-0.35},
                     "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":1.0,"uncertainty_stddev":1.5,
                       "uncertainty_component_correlation":0.4,"uncertainty_spatial_scale":10000.0,
                       "uncertainty_temporal_scale":3600.0}}})";
    }
    Config drift(leeway);
    assert(drift.Leeway() && drift.DriftObject()==DriftObjectType::KAYAK_WITH_PERSON);
    assert(drift.DefaultDriftSide()==DriftSide::RIGHT);
    assert(drift.LeewayCoefficientEnsemble());
    assert(drift.LeewayResidualCorrelation()==-.35 && drift.WindErrorStdDev()==1.5);
    assert(drift.WindErrorComponentCorrelation()==.4 && drift.WindErrorSpatialScale()==10000);
    assert(drift.WindErrorTemporalScale()==3600);
    drift.saveAsJson(saved);
    Config driftRestored(saved);
    assert(driftRestored.DriftObject()==DriftObjectType::KAYAK_WITH_PERSON);
    assert(driftRestored.LeewayCoefficientEnsemble());
    assert(driftRestored.LeewayResidualCorrelation()==-.35 && driftRestored.WindErrorStdDev()==1.5);
    assert(driftRestored.WindErrorComponentCorrelation()==.4 && driftRestored.WindErrorSpatialScale()==10000);
    assert(driftRestored.WindErrorTemporalScale()==3600);
    const string calibrated="config-test-calibrated.json";
    {
        std::ofstream file(calibrated);
        file << R"({"drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"right"},
          "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":0.0}},
          "observational_calibration":{"regions":[{"id":"bay-of-naples-piw-v1","crs":"EPSG:4326",
            "bounds":{"west":13.8,"south":40.5,"east":14.5,"north":41.1},
            "object_type":"PERSON_IN_WATER","forcing_adapter":"constant",
            "valid_from":"2020-01-01T00:00:00Z","valid_until":"2024-12-31T23:59:59Z",
            "estimator":"unbiased residual covariance after paired current-wind subtraction","sample_size":42,
            "dataset":"doi-archived paired drifter and forcing residuals",
            "dataset_checksum":"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
            "doi":"10.0000/example.calibration","wind_error":{"stddev":1.2,"component_correlation":0.25,
            "spatial_scale":8000.0,"temporal_scale":1800.0}}]}})";
    }
    Config regional(calibrated);
    assert(regional.ObservationalCalibrationRegionCount()==1);
    regional.saveAsJson(saved);
    Config regionalRestored(saved);
    assert(regionalRestored.ObservationalCalibrationRegionCount()==1);
    const string sideEnsemble="config-test-side-ensemble.json";
    {
        std::ofstream file(sideEnsemble);
        file << R"({"drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"random","side_right_probability":0.65,"jibe_probability_per_hour":0.04},
                     "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":1.0}}})";
    }
    Config randomSide(sideEnsemble);
    assert(randomSide.LeewayRandomSide() && randomSide.LeewayRightSideProbability()==.65);
    assert(randomSide.LeewayJibeProbabilityHourly()==.04);
    randomSide.saveAsJson(saved);
    Config randomSideRestored(saved);
    assert(randomSideRestored.LeewayRandomSide() && randomSideRestored.LeewayRightSideProbability()==.65);
    assert(randomSideRestored.LeewayJibeProbabilityHourly()==.04);
    const string environment="config-test-environment.json";
    {
        std::ofstream file(environment);
#ifdef WACOMM_USE_PROJ
        file << R"({"io":{"nc_inputs":["ocean.nc"]},
          "drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"left"},
          "environment":{"wind":{"adapter":"WRF","nc_inputs":["wrf.nc"],"regrid":"bilinear_curvilinear_geographic"},
                         "wave":{"adapter":"WW3","nc_inputs":["ww3.nc"],"regrid":"bilinear_projected","source_crs":"EPSG:3857"}}})";
#else
        file << R"({"io":{"nc_inputs":["ocean.nc"]},
          "drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"left"},
          "environment":{"wind":{"adapter":"WRF","nc_inputs":["wrf.nc"],"regrid":"bilinear_curvilinear_geographic"},
                         "wave":{"adapter":"WW3","nc_inputs":["ww3.nc"],"regrid":"bilinear_geographic"}}})";
#endif
    }
    Config environmental(environment);
    assert(environmental.WeatherModel()=="WRF" && environmental.WeatherInputs().size()==1);
    assert(environmental.WaveModel()=="WW3" && environmental.WaveInputs().size()==1);
    assert(environmental.WeatherRegridding()=="bilinear_curvilinear_geographic");
#ifdef WACOMM_USE_PROJ
    assert(environmental.WaveRegridding()=="bilinear_projected" && environmental.WaveSourceCrs()=="EPSG:3857");
#else
    assert(environmental.WaveRegridding()=="bilinear_geographic" && environmental.WaveSourceCrs().empty());
#endif
    assert(environmental.WeatherSourceCrs().empty());
    const string remoteEnvironment="config-test-remote-environment.json";
    {
        std::ofstream file(remoteEnvironment);
        file << R"({"io":{"base_path":"https://ocean.example.test/dap","nc_inputs":["ocean.nc"]},
          "environment":{"wind":{"adapter":"WRF","base_path":"https://weather.example.test/dap/","nc_inputs":["wrf.nc"]}}})";
    }
    Config remote(remoteEnvironment);
    assert(remote.NcInputs()[0]=="https://ocean.example.test/dap/ocean.nc");
    assert(remote.WeatherInputs()[0]=="https://weather.example.test/dap/wrf.nc");
    {
        std::ofstream file(invalid);
        file << R"({"physics":{"upper_closure":"unknown"}})";
    }
    bool rejected=false;
    try { Config invalidConfig(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"io":{"nc_inputs":["ocean.nc"]},"environment":{"wave":{"adapter":"WW3","nc_inputs":["ww3.nc"],"regrid":"bilinear_projected"}}})";
    }
    rejected=false;
    try { Config missingProjectedCrs(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"io":{"nc_inputs":["ocean.nc"]},"environment":{"wave":{"adapter":"WW3","nc_inputs":["ww3.nc"],"regrid":"automatic"}}})";
    }
    rejected=false;
    try { Config invalidRegridding(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"drift":{"model":"passive","coefficient_ensemble":true}})";
    }
    rejected=false;
    try { Config invalidPassiveEnsemble(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"right","coefficient_ensemble":true,"residual_correlation":1.1},
                     "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":0.0}}})";
    }
    rejected=false;
    try { Config invalidCorrelation(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"environment":{"wind":{"adapter":"constant","u10":5.0,"v10":0.0,"uncertainty_stddev":1.0}}})";
    }
    rejected=false;
    try { Config invalidPassiveWindError(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"right"},
                     "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":0.0,
                     "uncertainty_stddev":1.0,"uncertainty_component_correlation":1.01}}})";
    }
    rejected=false;
    try { Config invalidWindCovariance(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"right"},
                     "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":0.0,
                     "uncertainty_spatial_scale":1000.0}}})";
    }
    rejected=false;
    try { Config unusedWindCorrelation(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"random"},
                     "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":0.0}}})";
    }
    rejected=false;
    try { Config missingSideProbability(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"random","side_right_probability":1.1},
                     "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":0.0}}})";
    }
    rejected=false;
    try { Config invalidSideProbability(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"right","side_right_probability":0.5},
                     "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":0.0}}})";
    }
    rejected=false;
    try { Config unusedSideProbability(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalid);
        file << R"({"drift":{"model":"leeway","object_type":"PERSON_IN_WATER","side":"right","jibe_probability_per_hour":1.1},
                     "environment":{"wind":{"adapter":"constant","u10":5.0,"v10":0.0}}})";
    }
    rejected=false;
    try { Config invalidJibeProbability(invalid); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    {
        std::ofstream file(invalidStep);
        file << R"({"physics":{"dti":0}})";
    }
    rejected=false;
    try { Config invalidStepConfig(invalidStep); }
    catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    std::remove(input.c_str()); std::remove(saved.c_str()); std::remove(invalid.c_str());
    std::remove(invalidStep.c_str());
    std::remove(leeway.c_str());
    std::remove(calibrated.c_str());
    std::remove(environment.c_str());
    std::remove(sideEnsemble.c_str());
    std::remove(remoteEnvironment.c_str());
}
