cmake_minimum_required(VERSION 3.20)
project(DAQ LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_BUILD_TYPE Debug)

# Optimization flags
add_compile_options(-O3 -ffast-math)
add_compile_options(-std=c++23)

# MIDAS
if(NO_MIDAS)
   message(STATUS "${PROJECT_NAME}: MIDAS support is disabled via NO_MIDAS")
   unset(MIDAS_FOUND)
elseif(TARGET midas)
   message(STATUS "${PROJECT_NAME}: Building as a subproject of MIDAS")
   set(MIDAS_FOUND true)
   set(MIDASSYS ${CMAKE_HOME_DIRECTORY})
elseif(DEFINED ENV{MIDASSYS})
   include($ENV{MIDASSYS}/lib/midas-targets.cmake)
   include($ENV{MIDASSYS}/lib/manalyzer-targets.cmake)
   message(STATUS "${PROJECT_NAME}: Building with MIDAS from $ENV{MIDASSYS}/lib/midas-targets.cmake")
   set(MIDAS_FOUND TRUE)
   set(MIDASSYS $ENV{MIDASSYS})
else()
   message(STATUS "${PROJECT_NAME}: Building without MIDAS")
   unset(MIDAS_FOUND)
endif()
message(STATUS "${PROJECT_NAME}: MIDASSYS: ${MIDASSYS}")

# CAEN FELib  (required by both frontends)
# CAEN FElib
set(CAEN-FElib_ROOT "/work/mancx/packages/caen_felib-v1.3.2")
add_library(caen-felib SHARED IMPORTED)
set_target_properties(caen-felib PROPERTIES
    IMPORTED_LOCATION "${CAEN-FElib_ROOT}/lib/libCAEN_FELib.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CAEN-FElib_ROOT}/include"
)

# CAEN Dig2
set(CAEN-Dig2_ROOT "/work/mancx/packages/caen_dig2-v1.8.1")
add_library(caen-dig2 SHARED IMPORTED)
set_target_properties(caen-dig2 PROPERTIES
    IMPORTED_LOCATION "${CAEN-Dig2_ROOT}/lib/libCAEN_Dig2.so"
)

# LZ4  (indirect dependency of caen-dig2)
#find_library(LZ4_LIB lz4 REQUIRED)
#message(STATUS "${PROJECT_NAME}: LZ4: ${LZ4_LIB}")

# FFTW 
set(FFTW_ROOT "/work/mancx/packages/fftw-3.3.10")
add_library(fftw3 SHARED IMPORTED)
set_target_properties(fftw3 PROPERTIES
    IMPORTED_LOCATION "${FFTW_ROOT}/lib/libfftw3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${FFTW_ROOT}/include"
)

# Executables
add_executable(main src/main_midas.cpp)
#add_executable(main tests/mixer_test.cpp)
#add_executable(main tests/filter_test.cpp)
#add_executable(main tests/psd_test.cpp)

target_link_libraries(main
    PRIVATE
        fftw3
        caen-felib
        caen-dig2
        ${LZ4_LIB}
        midas::mfe
        m
)
