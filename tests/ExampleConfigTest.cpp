#include "../Config.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    if(argc != 2) return 2;
    unsigned int configurations = 0;
    unsigned int unsupported = 0;
    try {
        for(const auto &entry : std::filesystem::directory_iterator(argv[1])) {
            if(!entry.is_directory()) continue;
            for(const auto &file : std::filesystem::directory_iterator(entry.path())) {
                if(file.path().extension() != ".json") continue;
                try {
                    std::ifstream input(file.path());
                    nlohmann::json document;
                    input >> document;
                    if(document.contains("features") && document.value("type", "") == "FeatureCollection") continue;
#ifndef WACOMM_USE_PROJ
                    if(document.contains("environment") && document["environment"].contains("wave") &&
                       document["environment"]["wave"].value("regrid", "") == "bilinear_projected") {
                        std::cout << "Requires USE_PROJ=ON: " << file.path() << std::endl;
                        ++unsupported;
                        continue;
                    }
#endif
                    Config configuration(file.path().string());
                    ++configurations;
                } catch(const std::exception &error) {
                    std::cerr << file.path() << ": " << error.what() << std::endl;
                    return 1;
                }
            }
        }
    } catch(const std::exception &error) {
        std::cerr << "Example configuration validation failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Validated " << configurations << " example simulation configurations; "
              << unsupported << " require a different build" << std::endl;
    return configurations == 0 ? 1 : 0;
}
