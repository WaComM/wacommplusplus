#include "../Config.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <stdexcept>

int main() {
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
    {
        std::ofstream file(invalid);
        file << R"({"physics":{"upper_closure":"unknown"}})";
    }
    bool rejected=false;
    try { Config invalidConfig(invalid); }
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
}
