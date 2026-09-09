//
// Created by Raffaele Montella on 9/9/26.
//

#ifndef WACOMMPLUSPLUS_PROVENANCE_HPP
#define WACOMMPLUSPLUS_PROVENANCE_HPP

#include "BuildInfo.hpp"
#include "Config.hpp"

#include <netcdf>

namespace Provenance {

    inline void writeBuildMetadata(netCDF::NcFile &file, Config &config) {
        file.putAtt("wacomm_git_revision",WACOMM_GIT_REVISION);
        file.putAtt("wacomm_compiler",WACOMM_COMPILER);
        file.putAtt("wacomm_cmake_options",WACOMM_CMAKE_OPTIONS);
        file.putAtt("wacomm_configuration_file",config.ConfigFile());
        file.putAtt("wacomm_configuration",config.asJson());
    }
}

#endif //WACOMMPLUSPLUS_PROVENANCE_HPP
