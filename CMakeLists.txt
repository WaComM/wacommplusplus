cmake_minimum_required(VERSION 3.20)
project(wacommplusplus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEBUG "Compile logging information in performance-critical classes." OFF)
option(BUILD_APPLICATION "Build the WaComM++ application." ON)
option(BUILD_TESTING "Build the test suite." ON)
option(USE_MPI "Use MPI for distributed memory parallelism." OFF)
option(USE_EMPI "Use FlexMPI/EMPI for malleable distributed memory parallelism." OFF)
option(USE_OMP "Use OpenMP for shared memory parallelism." OFF)
option(USE_OPENACC "Use OpenACC acceleration." OFF)
option(USE_CUDA "Use CUDA acceleration." OFF)

if(USE_MPI AND USE_EMPI)
    message(FATAL_ERROR "You can not use both MPI and EMPI.")
endif()

if(DEBUG)
    add_compile_definitions(DEBUG)
endif()

if(BUILD_APPLICATION)
    find_package(Threads REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NETCDF REQUIRED IMPORTED_TARGET netcdf)
    pkg_check_modules(LOG4CPLUS REQUIRED IMPORTED_TARGET log4cplus)
    find_path(NETCDF_CXX_INCLUDE_DIR netcdf PATH_SUFFIXES include)
    find_library(NETCDF_CXX_LIBRARY NAMES netcdf-cxx4)
    if(NOT NETCDF_CXX_INCLUDE_DIR OR NOT NETCDF_CXX_LIBRARY)
        message(FATAL_ERROR "NetCDF C++4 headers and library were not found.")
    endif()
    add_library(NetCDF::CXX4 UNKNOWN IMPORTED)
    set_target_properties(NetCDF::CXX4 PROPERTIES IMPORTED_LOCATION "${NETCDF_CXX_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${NETCDF_CXX_INCLUDE_DIR}")
    set(WACOMM_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS}
            nlohmann_json::nlohmann_json NetCDF::CXX4 PkgConfig::NETCDF PkgConfig::LOG4CPLUS)

    if(USE_OMP)
        find_package(OpenMP REQUIRED COMPONENTS CXX)
        list(APPEND WACOMM_LIBRARIES OpenMP::OpenMP_CXX)
        add_compile_definitions(USE_OMP)
    endif()

    if(USE_MPI OR USE_EMPI)
        find_package(MPI REQUIRED COMPONENTS C)
        list(APPEND WACOMM_LIBRARIES MPI::MPI_C)
        if(USE_MPI)
            add_compile_definitions(USE_MPI)
        else()
            add_compile_definitions(USE_EMPI)
        endif()
    endif()

    if(USE_OPENACC)
        find_package(OpenACC REQUIRED)
        list(APPEND WACOMM_LIBRARIES OpenACC::OpenACC_CXX)
        add_compile_definitions(USE_OPENACC)
    endif()

    set(WACOMM_CUDA_SOURCES)
    if(USE_CUDA)
        if(APPLE)
            message(FATAL_ERROR "CUDA is not supported on modern macOS.")
        endif()
        enable_language(CUDA)
        find_package(CUDAToolkit REQUIRED)
        set(CMAKE_CUDA_STANDARD 17)
        set(CMAKE_CUDA_STANDARD_REQUIRED ON)
        list(APPEND WACOMM_CUDA_SOURCES cuda/kernel.cu cuda/kernel.h)
        list(APPEND WACOMM_LIBRARIES CUDA::cudart CUDA::curand)
        add_compile_definitions(USE_CUDA)
    endif()

    add_executable(wacommplusplus
            main.cpp Particle.cpp Particles.cpp Wacomm.cpp Source.cpp Sources.cpp
            Config.cpp OceanModelAdapter.cpp Utils.cpp WacommPlusPlus.cpp JulianDate.cpp
            OceanModelAdapters/ROMSAdapter.cpp OceanModelAdapters/WacommAdapter.cpp
            OceanModelAdapters/StructuredGridAdapter.cpp
            ${WACOMM_CUDA_SOURCES})
    target_link_libraries(wacommplusplus PRIVATE ${WACOMM_LIBRARIES})
    install(TARGETS wacommplusplus RUNTIME DESTINATION bin)

    if(BUILD_TESTING)
        add_executable(structured_grid_adapter_test tests/StructuredGridAdapterTest.cpp
                OceanModelAdapter.cpp OceanModelAdapters/StructuredGridAdapter.cpp)
        target_link_libraries(structured_grid_adapter_test PRIVATE ${WACOMM_LIBRARIES})
        add_test(NAME structured_grid_adapters COMMAND structured_grid_adapter_test)
    endif()
endif()

if(BUILD_TESTING)
    include(CTest)
    add_executable(numerical_helpers_test tests/NumericalHelpersTest.cpp)
    target_include_directories(numerical_helpers_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME numerical_helpers COMMAND numerical_helpers_test)
endif()
