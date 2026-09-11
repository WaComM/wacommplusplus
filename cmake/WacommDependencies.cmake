include(FetchContent)
include(ExternalProject)

function(wacomm_configure_dependencies result)
    set(force_all FALSE)
    if(WACOMM_BOOTSTRAP_DEPENDENCIES STREQUAL "ON")
        set(force_all TRUE)
    endif()
    set(force_io ${force_all})
    if(WACOMM_BOOTSTRAP_PARALLEL_IO)
        set(force_io TRUE)
        find_package(MPI REQUIRED COMPONENTS C CXX)
    endif()

    set(prefix "${CMAKE_BINARY_DIR}/external")
    set(source "${prefix}/src")
    set(build "${prefix}/build")
    file(MAKE_DIRECTORY "${prefix}/include" "${prefix}/lib")

    # ON deliberately skips discovery: a private build must not silently select
    # headers from one installation and libraries from another.
    if(NOT force_all)
        find_package(nlohmann_json 3.11.3 CONFIG QUIET)
    endif()
    if(NOT TARGET nlohmann_json::nlohmann_json)
        if(WACOMM_BOOTSTRAP_DEPENDENCIES STREQUAL "OFF")
            message(FATAL_ERROR "nlohmann/json >= 3.11.3 was not found and dependency bootstrap is OFF.")
        endif()
        FetchContent_Declare(wacomm_json
                URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
                URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(wacomm_json)
    endif()

    set(private_io ${force_io})
    if(NOT private_io)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(WACOMM_NETCDF QUIET IMPORTED_TARGET netcdf)
        endif()
        find_path(WACOMM_NETCDF_CXX_INCLUDE netcdf)
        find_library(WACOMM_NETCDF_CXX_LIBRARY NAMES netcdf-cxx4 netcdf_c++4)
        if(TARGET PkgConfig::WACOMM_NETCDF AND WACOMM_NETCDF_CXX_INCLUDE AND WACOMM_NETCDF_CXX_LIBRARY)
            include(CheckCXXSourceCompiles)
            set(CMAKE_REQUIRED_INCLUDES "${WACOMM_NETCDF_INCLUDE_DIRS}")
            check_cxx_source_compiles("#include <netcdf_meta.h>
                #if !NC_HAS_DAP2 || !NC_HAS_DAP4
                #error DAP2 and DAP4 are required
                #endif
                int main() { return 0; }" WACOMM_NETCDF_HAS_DAP)
            unset(CMAKE_REQUIRED_INCLUDES)
        endif()
        if(NOT WACOMM_NETCDF_HAS_DAP)
            if(WACOMM_BOOTSTRAP_DEPENDENCIES STREQUAL "OFF")
                message(FATAL_ERROR "Installed NetCDF-C and NetCDF-C++4 with DAP2/DAP4 are required when dependency bootstrap is OFF.")
            endif()
            set(private_io TRUE)
        endif()
    endif()

    set(common -DCMAKE_INSTALL_PREFIX:PATH=${prefix} -DCMAKE_INSTALL_LIBDIR:PATH=lib
            -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
            -DBUILD_SHARED_LIBS:BOOL=OFF -DBUILD_TESTING:BOOL=OFF
            -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5)
    if(CMAKE_CONFIGURATION_TYPES)
        list(APPEND common -DCMAKE_CONFIGURATION_TYPES:STRING=Release)
        set(build_cmd ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --parallel)
        set(install_cmd ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release)
    else()
        list(APPEND common -DCMAKE_BUILD_TYPE:STRING=Release)
        set(build_cmd ${CMAKE_COMMAND} --build <BINARY_DIR> --parallel)
        set(install_cmd ${CMAKE_COMMAND} --install <BINARY_DIR>)
    endif()

    if(private_io)
        # The HDF5/NetCDF chain is kept coherent because its static library ABI
        # and feature set cannot safely be assembled from arbitrary installations.
        if(WIN32)
            set(zlib "${prefix}/lib/zlibstatic.lib")
            set(sz "${prefix}/lib/libsz.lib")
            set(curl "${prefix}/lib/libcurl.lib")
            set(hdf5 "${prefix}/lib/libhdf5-static.lib")
            set(hdf5_hl "${prefix}/lib/libhdf5_hl-static.lib")
            set(netcdf "${prefix}/lib/netcdf.lib")
            set(netcdf_cxx "${prefix}/lib/netcdf-cxx4.lib")
        else()
            set(zlib "${prefix}/lib/libz.a")
            set(sz "${prefix}/lib/libsz.a")
            set(curl "${prefix}/lib/libcurl.a")
            set(hdf5 "${prefix}/lib/libhdf5.a")
            set(hdf5_hl "${prefix}/lib/libhdf5_hl.a")
            set(netcdf "${prefix}/lib/libnetcdf.a")
            set(netcdf_cxx "${prefix}/lib/libnetcdf-cxx4.a")
        endif()
        ExternalProject_Add(wacomm_zlib URL https://zlib.net/fossils/zlib-1.3.1.tar.gz
                URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_DIR "${source}/zlib" BINARY_DIR "${build}/zlib"
                CMAKE_ARGS ${common} -DZLIB_BUILD_EXAMPLES:BOOL=OFF BUILD_COMMAND ${build_cmd}
                INSTALL_COMMAND ${install_cmd} BUILD_BYPRODUCTS "${zlib}")
        # libaec is the maintained SZIP-compatible implementation and supports
        # native MSVC generators; no Unix shell or obsolete VS project is needed.
        ExternalProject_Add(wacomm_libaec
                URL https://github.com/MathisRosenhauer/libaec/releases/download/v1.1.3/libaec-1.1.3.tar.gz
                URL_HASH SHA256=bd8bea8b69ca602796b2daf17b0a7de019ce3c3bd0ad56fa9ef4a631a4088058
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_DIR "${source}/libaec" BINARY_DIR "${build}/libaec"
                CMAKE_ARGS ${common} BUILD_COMMAND ${build_cmd} INSTALL_COMMAND ${install_cmd}
                BUILD_BYPRODUCTS "${sz}")
        set(curl_tls)
        set(curl_system)
        set(curl_dependencies wacomm_zlib)
        if(WIN32)
            # Schannel keeps HTTPS enabled without requiring OpenSSL on MSVC.
            set(curl_tls -DCURL_USE_SCHANNEL:BOOL=ON -DCURL_USE_OPENSSL:BOOL=OFF)
            set(curl_system ws2_32 crypt32 secur32 advapi32 normaliz wldap32)
        elseif(APPLE)
            set(curl_tls -DCURL_USE_SECTRANSP:BOOL=ON -DCURL_USE_OPENSSL:BOOL=OFF)
            find_library(WACOMM_SECURITY_FRAMEWORK Security REQUIRED)
            find_library(WACOMM_COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
            find_library(WACOMM_SYSTEMCONFIGURATION_FRAMEWORK SystemConfiguration REQUIRED)
            set(curl_system ${WACOMM_SECURITY_FRAMEWORK} ${WACOMM_COREFOUNDATION_FRAMEWORK}
                    ${WACOMM_SYSTEMCONFIGURATION_FRAMEWORK})
        else()
            find_package(Perl REQUIRED)
            find_program(WACOMM_OPENSSL_MAKE NAMES gmake make REQUIRED)
            if(CMAKE_CROSSCOMPILING)
                message(FATAL_ERROR "Private OpenSSL requires a native Unix build; use installed dependencies for cross-compilation.")
            endif()
            set(openssl_ssl "${prefix}/lib/libssl.a")
            set(openssl_crypto "${prefix}/lib/libcrypto.a")
            ExternalProject_Add(wacomm_openssl
                    URL https://github.com/openssl/openssl/releases/download/openssl-${WACOMM_OPENSSL_VERSION}/openssl-${WACOMM_OPENSSL_VERSION}.tar.gz
                    URL_HASH SHA256=a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2
                    DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_DIR "${source}/openssl" BINARY_DIR "${build}/openssl"
                    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}"
                        "AR=${CMAKE_AR}" "RANLIB=${CMAKE_RANLIB}"
                        ${PERL_EXECUTABLE} <SOURCE_DIR>/Configure
                        --prefix=${prefix} --openssldir=${prefix}/ssl --libdir=lib
                        no-shared no-module no-tests -fPIC
                    BUILD_COMMAND ${WACOMM_OPENSSL_MAKE}
                    INSTALL_COMMAND ${WACOMM_OPENSSL_MAKE} install_sw
                    BUILD_BYPRODUCTS "${openssl_ssl}" "${openssl_crypto}")
            # Pin both headers and archives so a loaded module cannot supply
            # a different OpenSSL ABI to curl or the final application.
            # Normal include paths take precedence over module-provided CPATH.
            set(curl_tls -DCURL_USE_OPENSSL:BOOL=ON -DOPENSSL_ROOT_DIR:PATH=${prefix}
                    -DOPENSSL_INCLUDE_DIR:PATH=${prefix}/include
                    -DOPENSSL_SSL_LIBRARY:FILEPATH=${openssl_ssl}
                    -DOPENSSL_CRYPTO_LIBRARY:FILEPATH=${openssl_crypto}
                    -DOPENSSL_USE_STATIC_LIBS:BOOL=ON
                    -DCMAKE_NO_SYSTEM_FROM_IMPORTED:BOOL=ON)
            set(curl_system "${openssl_ssl}" "${openssl_crypto}" Threads::Threads ${CMAKE_DL_LIBS})
            list(APPEND curl_dependencies wacomm_openssl)
            message(STATUS "WaComM++ private OpenSSL=${WACOMM_OPENSSL_VERSION}")
        endif()
        ExternalProject_Add(wacomm_curl URL https://curl.se/download/curl-8.7.1.tar.xz
                URL_HASH SHA256=6fea2aac6a4610fbd0400afb0bcddbe7258a64c63f1f68e5855ebc0c659710cd
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_DIR "${source}/curl" BINARY_DIR "${build}/curl"
                CMAKE_ARGS ${common} ${curl_tls} -DBUILD_CURL_EXE:BOOL=OFF -DBUILD_EXAMPLES:BOOL=OFF
                    -DBUILD_LIBCURL_DOCS:BOOL=OFF -DCURL_ZLIB:BOOL=ON -DZLIB_ROOT:PATH=${prefix}
                    -DCURL_USE_LIBSSH2:BOOL=OFF -DCURL_USE_LIBSSH:BOOL=OFF
                    -DCURL_USE_LIBPSL:BOOL=OFF -DUSE_LIBIDN2:BOOL=OFF
                    -DCURL_BROTLI:BOOL=OFF -DCURL_ZSTD:BOOL=OFF -DCURL_DISABLE_LDAP:BOOL=ON
                BUILD_COMMAND ${build_cmd} INSTALL_COMMAND ${install_cmd} DEPENDS ${curl_dependencies}
                BUILD_BYPRODUCTS "${curl}")
        set(hdf5_parallel -DHDF5_ENABLE_PARALLEL:BOOL=OFF -DHDF5_BUILD_CPP_LIB:BOOL=ON)
        set(netcdf_parallel -DENABLE_PARALLEL4:BOOL=OFF)
        if(WACOMM_BOOTSTRAP_PARALLEL_IO)
            # Parallel HDF5 uses the discovered MPI compiler. Its unused C++ API
            # is disabled because the pinned HDF5 release does not combine it
            # with parallel mode.
            set(hdf5_parallel -DHDF5_ENABLE_PARALLEL:BOOL=ON -DHDF5_BUILD_CPP_LIB:BOOL=OFF
                    -DCMAKE_C_COMPILER:FILEPATH=${MPI_C_COMPILER})
            set(netcdf_parallel -DENABLE_PARALLEL4:BOOL=ON -DHDF5_PARALLEL:BOOL=ON
                    -DUSE_PARALLEL:BOOL=ON -DUSE_PARALLEL4:BOOL=ON
                    -DCMAKE_C_FLAGS:STRING=-DHDF5_PARALLEL
                    -DCMAKE_C_COMPILER:FILEPATH=${MPI_C_COMPILER})
        endif()
        ExternalProject_Add(wacomm_hdf5
                URL https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.12/hdf5-1.12.0/src/hdf5-1.12.0.tar.gz
                URL_HASH SHA256=a62dcb276658cb78e6795dd29bf926ed7a9bc4edf6e77025cd2c689a8f97c17a
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_DIR "${source}/hdf5" BINARY_DIR "${build}/hdf5"
                CMAKE_ARGS ${common} ${hdf5_parallel} -DHDF5_BUILD_HL_LIB:BOOL=ON -DHDF5_BUILD_TOOLS:BOOL=OFF
                    -DHDF5_ENABLE_Z_LIB_SUPPORT:BOOL=ON -DZLIB_ROOT:PATH=${prefix}
                    -DHDF5_ENABLE_SZIP_SUPPORT:BOOL=ON -DSZIP_INCLUDE_DIR:PATH=${prefix}/include
                    -DSZIP_LIBRARY:FILEPATH=${sz}
                BUILD_COMMAND ${build_cmd} INSTALL_COMMAND ${install_cmd} DEPENDS wacomm_zlib wacomm_libaec
                BUILD_BYPRODUCTS "${hdf5}" "${hdf5_hl}")
        ExternalProject_Add(wacomm_netcdf_c
                URL https://downloads.unidata.ucar.edu/netcdf-c/4.8.1/netcdf-c-4.8.1.tar.gz
                URL_HASH SHA256=808ac326ddecc8e0bb70f3ba35747fd6907f2b446817ce1465219974abe472b1
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_DIR "${source}/netcdf-c" BINARY_DIR "${build}/netcdf-c"
                # The official 4.8.1 release archive omits the optional fuzz
                # submodule although its CMake file enters that directory.
                PATCH_COMMAND ${CMAKE_COMMAND} -E make_directory <SOURCE_DIR>/fuzz
                    COMMAND ${CMAKE_COMMAND} -E touch <SOURCE_DIR>/fuzz/CMakeLists.txt
                CMAKE_ARGS ${common} ${netcdf_parallel} -DENABLE_NETCDF_4:BOOL=ON -DENABLE_DAP:BOOL=ON
                    -DENABLE_BYTERANGE:BOOL=ON -DENABLE_NCZARR:BOOL=OFF
                    -DENABLE_TESTS:BOOL=OFF -DBUILD_UTILITIES:BOOL=OFF
                    -DHDF5_ROOT:PATH=${prefix} -DCURL_ROOT:PATH=${prefix}
                    -DCURL_LIBRARY:FILEPATH=${curl} -DCURL_INCLUDE_DIR:PATH=${prefix}/include
                    -DZLIB_ROOT:PATH=${prefix} -DZLIB_LIBRARY:FILEPATH=${zlib}
                    -DZLIB_INCLUDE_DIR:PATH=${prefix}/include -DSZIP_LIBRARY:FILEPATH=${sz}
                    -DSZIP_INCLUDE_DIR:PATH=${prefix}/include
                BUILD_COMMAND ${build_cmd} INSTALL_COMMAND ${install_cmd} DEPENDS wacomm_hdf5 wacomm_curl
                BUILD_BYPRODUCTS "${netcdf}")
        ExternalProject_Add(wacomm_netcdf_cxx
                URL https://downloads.unidata.ucar.edu/netcdf-cxx/4.3.1/netcdf-cxx4-4.3.1.tar.gz
                URL_HASH SHA256=6a1189a181eed043b5859e15d5c080c30d0e107406fbb212c8fb9814e90f3445
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_DIR "${source}/netcdf-cxx" BINARY_DIR "${build}/netcdf-cxx"
                CMAKE_ARGS ${common} -DCMAKE_PREFIX_PATH:PATH=${prefix}
                    -DnetCDF_LIBRARIES:FILEPATH=${netcdf} -DnetCDF_INCLUDE_DIR:PATH=${prefix}/include
                    -DHDF5_DIR:PATH=${prefix}/share/cmake/hdf5
                    -DHDF5_C_LIBRARY_hdf5:FILEPATH=${hdf5}
                    -DHDF5_C_LIBRARY_hdf5_hl:FILEPATH=${hdf5_hl}
                    -DNCXX_ENABLE_TESTS:BOOL=OFF -DENABLE_DOXYGEN:BOOL=OFF
                BUILD_COMMAND ${build_cmd} INSTALL_COMMAND ${install_cmd} DEPENDS wacomm_netcdf_c
                BUILD_BYPRODUCTS "${netcdf_cxx}")
        add_library(wacomm_private_io INTERFACE)
        add_dependencies(wacomm_private_io wacomm_netcdf_cxx)
        target_include_directories(wacomm_private_io INTERFACE "${prefix}/include")
        target_link_libraries(wacomm_private_io INTERFACE "${netcdf_cxx}" "${netcdf}" "${hdf5_hl}"
                "${hdf5}" "${curl}" "${zlib}" "${sz}" ${curl_system})
        # Static parallel I/O carries MPI symbols even if solver decomposition
        # is disabled, so the dependency propagates independently of USE_MPI.
        if(WACOMM_BOOTSTRAP_PARALLEL_IO)
            target_link_libraries(wacomm_private_io INTERFACE MPI::MPI_C MPI::MPI_CXX)
        endif()
        set(io_targets wacomm_private_io)
    else()
        add_library(WaComMNetCDFCxx4 UNKNOWN IMPORTED)
        set_target_properties(WaComMNetCDFCxx4 PROPERTIES IMPORTED_LOCATION "${WACOMM_NETCDF_CXX_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${WACOMM_NETCDF_CXX_INCLUDE}")
        set(io_targets WaComMNetCDFCxx4 PkgConfig::WACOMM_NETCDF)
    endif()

    if(NOT force_all)
        find_package(log4cplus CONFIG QUIET)
        if(NOT TARGET log4cplus::log4cplus AND PkgConfig_FOUND)
            pkg_check_modules(WACOMM_LOG4CPLUS QUIET IMPORTED_TARGET log4cplus)
        endif()
    endif()
    if(TARGET log4cplus::log4cplus)
        set(logging log4cplus::log4cplus)
    elseif(TARGET PkgConfig::WACOMM_LOG4CPLUS)
        set(logging PkgConfig::WACOMM_LOG4CPLUS)
    else()
        if(WACOMM_BOOTSTRAP_DEPENDENCIES STREQUAL "OFF")
            message(FATAL_ERROR "log4cplus was not found and dependency bootstrap is OFF.")
        endif()
        if(WIN32)
            set(log_library "${prefix}/lib/log4cplusS.lib")
        else()
            set(log_library "${prefix}/lib/liblog4cplusS.a")
        endif()
        ExternalProject_Add(wacomm_log4cplus
                URL https://github.com/log4cplus/log4cplus/releases/download/REL_2_1_1/log4cplus-2.1.1.tar.gz
                URL_HASH SHA256=42dc435928917fd2f847046c4a0c6086b2af23664d198c7fc1b982c0bfe600c1
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_DIR "${source}/log4cplus" BINARY_DIR "${build}/log4cplus"
                CMAKE_ARGS ${common} -DLOG4CPLUS_BUILD_TESTING:BOOL=OFF -DLOG4CPLUS_BUILD_EXAMPLES:BOOL=OFF
                BUILD_COMMAND ${build_cmd} INSTALL_COMMAND ${install_cmd} BUILD_BYPRODUCTS "${log_library}")
        add_library(wacomm_private_log INTERFACE)
        add_dependencies(wacomm_private_log wacomm_log4cplus)
        target_include_directories(wacomm_private_log INTERFACE "${prefix}/include")
        target_link_libraries(wacomm_private_log INTERFACE "${log_library}")
        set(logging wacomm_private_log)
    endif()
    message(STATUS "WaComM++ bootstrap=${WACOMM_BOOTSTRAP_DEPENDENCIES}, private I/O=${private_io}, parallel I/O=${WACOMM_BOOTSTRAP_PARALLEL_IO}")
    set(${result} ${io_targets} ${logging} PARENT_SCOPE)
endfunction()
