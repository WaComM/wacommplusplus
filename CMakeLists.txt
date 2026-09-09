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

set(WACOMM_GIT_REVISION "unknown")
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE WACOMM_GIT_RESULT
            OUTPUT_VARIABLE WACOMM_GIT_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    if(WACOMM_GIT_RESULT EQUAL 0)
        set(WACOMM_GIT_REVISION "${WACOMM_GIT_OUTPUT}")
    endif()
endif()
set(WACOMM_CMAKE_OPTIONS
        "BUILD_APPLICATION=${BUILD_APPLICATION};BUILD_TESTING=${BUILD_TESTING};DEBUG=${DEBUG};USE_MPI=${USE_MPI};USE_EMPI=${USE_EMPI};USE_OMP=${USE_OMP};USE_OPENACC=${USE_OPENACC};USE_CUDA=${USE_CUDA}")
set(WACOMM_GENERATED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(BuildInfo.hpp.in "${WACOMM_GENERATED_INCLUDE}/BuildInfo.hpp" @ONLY)

function(wacomm_enable_test_assertions target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
endfunction()

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
    find_library(NETCDF_CXX_LIBRARY NAMES netcdf-cxx4 netcdf_c++4)
    if(NOT NETCDF_CXX_INCLUDE_DIR OR NOT NETCDF_CXX_LIBRARY)
        message(FATAL_ERROR "NetCDF C++4 headers and library were not found.")
    endif()
    add_library(NetCDF::CXX4 UNKNOWN IMPORTED)
    set_target_properties(NetCDF::CXX4 PROPERTIES IMPORTED_LOCATION "${NETCDF_CXX_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${NETCDF_CXX_INCLUDE_DIR}")
    set(WACOMM_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS}
            nlohmann_json::nlohmann_json NetCDF::CXX4 PkgConfig::NETCDF PkgConfig::LOG4CPLUS)

    if(USE_OMP)
        if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
            find_program(HOMEBREW_EXECUTABLE brew)
            if(HOMEBREW_EXECUTABLE)
                execute_process(COMMAND "${HOMEBREW_EXECUTABLE}" --prefix libomp
                        RESULT_VARIABLE HOMEBREW_LIBOMP_RESULT
                        OUTPUT_VARIABLE HOMEBREW_LIBOMP_PREFIX
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)
                if(HOMEBREW_LIBOMP_RESULT EQUAL 0
                        AND EXISTS "${HOMEBREW_LIBOMP_PREFIX}/include/omp.h"
                        AND EXISTS "${HOMEBREW_LIBOMP_PREFIX}/lib/libomp.dylib")
                    if(NOT OpenMP_CXX_FLAGS)
                        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp" CACHE STRING
                                "C++ compiler flags for OpenMP" FORCE)
                    endif()
                    if(NOT OpenMP_CXX_INCLUDE_DIR)
                        set(OpenMP_CXX_INCLUDE_DIR "${HOMEBREW_LIBOMP_PREFIX}/include" CACHE PATH
                                "C++ include directory for OpenMP" FORCE)
                    endif()
                    if(NOT OpenMP_CXX_LIB_NAMES)
                        set(OpenMP_CXX_LIB_NAMES omp CACHE STRING
                                "C++ libraries for OpenMP" FORCE)
                    endif()
                    if(NOT OpenMP_omp_LIBRARY)
                        set(OpenMP_omp_LIBRARY "${HOMEBREW_LIBOMP_PREFIX}/lib/libomp.dylib" CACHE FILEPATH
                                "OpenMP runtime library" FORCE)
                    endif()
                endif()
            endif()
        endif()
        find_package(OpenMP REQUIRED COMPONENTS CXX)
        list(APPEND WACOMM_LIBRARIES OpenMP::OpenMP_CXX)
        add_compile_definitions(USE_OMP)
    endif()

    if(USE_MPI OR USE_EMPI)
        find_package(MPI REQUIRED COMPONENTS CXX)
        list(APPEND WACOMM_LIBRARIES MPI::MPI_CXX)
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
        list(APPEND WACOMM_LIBRARIES CUDA::cudart)
        add_compile_definitions(USE_CUDA)
    endif()

    add_executable(wacommplusplus
            main.cpp Particle.cpp Particles.cpp Wacomm.cpp Source.cpp Sources.cpp
            Config.cpp OceanModelAdapter.cpp OceanModelAdapterFactory.cpp Utils.cpp WacommPlusPlus.cpp JulianDate.cpp
            OceanModelAdapters/ROMSAdapter.cpp OceanModelAdapters/WacommAdapter.cpp
            OceanModelAdapters/StructuredGridAdapter.cpp
            ${WACOMM_CUDA_SOURCES})
    target_link_libraries(wacommplusplus PRIVATE ${WACOMM_LIBRARIES})
    target_include_directories(wacommplusplus PRIVATE "${WACOMM_GENERATED_INCLUDE}")
    install(TARGETS wacommplusplus RUNTIME DESTINATION bin)

    if(BUILD_TESTING)
        add_executable(structured_grid_adapter_test tests/StructuredGridAdapterTest.cpp
                OceanModelAdapter.cpp OceanModelAdapters/StructuredGridAdapter.cpp)
        wacomm_enable_test_assertions(structured_grid_adapter_test)
        target_link_libraries(structured_grid_adapter_test PRIVATE ${WACOMM_LIBRARIES})
        add_test(NAME structured_grid_adapters COMMAND structured_grid_adapter_test)

        add_executable(roms_adapter_test tests/ROMSAdapterTest.cpp
                OceanModelAdapter.cpp OceanModelAdapters/ROMSAdapter.cpp)
        wacomm_enable_test_assertions(roms_adapter_test)
        target_link_libraries(roms_adapter_test PRIVATE ${WACOMM_LIBRARIES})
        add_test(NAME roms_adapter COMMAND roms_adapter_test)

        add_executable(native_adapter_test tests/NativeAdapterTest.cpp
                OceanModelAdapter.cpp OceanModelAdapters/WacommAdapter.cpp)
        wacomm_enable_test_assertions(native_adapter_test)
        target_link_libraries(native_adapter_test PRIVATE ${WACOMM_LIBRARIES})
        add_test(NAME native_adapter COMMAND native_adapter_test)

        add_executable(config_test tests/ConfigTest.cpp Config.cpp Utils.cpp JulianDate.cpp)
        wacomm_enable_test_assertions(config_test)
        target_link_libraries(config_test PRIVATE ${WACOMM_LIBRARIES})
        add_test(NAME configuration_round_trip COMMAND config_test)

        add_executable(restart_test tests/RestartTest.cpp Particles.cpp Particle.cpp OceanModelAdapter.cpp
                Config.cpp Utils.cpp JulianDate.cpp)
        target_include_directories(restart_test PRIVATE "${WACOMM_GENERATED_INCLUDE}")
        wacomm_enable_test_assertions(restart_test)
        target_link_libraries(restart_test PRIVATE ${WACOMM_LIBRARIES})
        add_test(NAME restart_format COMMAND restart_test)

        add_executable(particle_move_test tests/ParticleMoveTest.cpp Particle.cpp)
        wacomm_enable_test_assertions(particle_move_test)
        target_link_libraries(particle_move_test PRIVATE ${WACOMM_LIBRARIES})
        add_test(NAME particle_physical_interval COMMAND particle_move_test)

        add_executable(concentration_test tests/ConcentrationTest.cpp Particles.cpp Particle.cpp
                OceanModelAdapter.cpp Config.cpp Utils.cpp JulianDate.cpp)
        target_include_directories(concentration_test PRIVATE "${WACOMM_GENERATED_INCLUDE}")
        wacomm_enable_test_assertions(concentration_test)
        target_link_libraries(concentration_test PRIVATE ${WACOMM_LIBRARIES})
        add_test(NAME concentration_preservation COMMAND concentration_test)

        if(USE_MPI)
            add_executable(mpi_decomposition_test tests/MpiDecompositionTest.cpp Particle.cpp)
            wacomm_enable_test_assertions(mpi_decomposition_test)
            target_link_libraries(mpi_decomposition_test PRIVATE ${WACOMM_LIBRARIES})
            add_test(NAME mpi_decomposition_parity COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 2
                    ${MPIEXEC_PREFLAGS} $<TARGET_FILE:mpi_decomposition_test> ${MPIEXEC_POSTFLAGS})
        endif()
    endif()
endif()

if(BUILD_TESTING)
    include(CTest)
    add_executable(numerical_helpers_test tests/NumericalHelpersTest.cpp)
    wacomm_enable_test_assertions(numerical_helpers_test)
    target_include_directories(numerical_helpers_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME numerical_helpers COMMAND numerical_helpers_test)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME reproducibility_manifest COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ReproducibilityManifestTest.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/reproducibility_manifest.py")
        add_test(NAME example_documentation COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ExampleDocumentationTest.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    endif()
endif()
