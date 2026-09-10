//
// Created by Raffaele Montella on 9/9/26.
//

#ifndef WACOMMPLUSPLUS_PROVENANCE_HPP
#define WACOMMPLUSPLUS_PROVENANCE_HPP

#include "BuildInfo.hpp"
#include "Config.hpp"

#include <netcdf>
#ifdef WACOMM_USE_PROJ
#include <proj.h>
#endif

namespace Provenance {

    inline void writeBuildMetadata(netCDF::NcFile &file, Config &config) {
        file.putAtt("wacomm_git_revision",WACOMM_GIT_REVISION);
        file.putAtt("wacomm_compiler",WACOMM_COMPILER);
        file.putAtt("wacomm_cmake_options",WACOMM_CMAKE_OPTIONS);
        file.putAtt("wacomm_configuration_file",config.ConfigFile());
        file.putAtt("wacomm_configuration",config.asJson());
#ifdef WACOMM_USE_PROJ
        file.putAtt("wacomm_proj_version",proj_info().version);
#endif
    }
}

#endif //WACOMMPLUSPLUS_PROVENANCE_HPP
