cmake_minimum_required(VERSION 3.16)
project(wfm_receiver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HACKRF REQUIRED libhackrf)
pkg_check_modules(ALSA REQUIRED alsa)

find_package(Boost 1.75 REQUIRED COMPONENTS json program_options)
find_package(Threads REQUIRED)

# libiio ships no pkg-config/CMake package config on Ubuntu, so locate it manually.
find_path(IIO_INCLUDE_DIR NAMES iio.h)
find_library(IIO_LIBRARY NAMES iio)
if(NOT IIO_INCLUDE_DIR OR NOT IIO_LIBRARY)
    message(FATAL_ERROR "libiio not found (install libiio-dev)")
endif()

add_library(wfm_hw
    src/hw/signal_source.cpp
    src/hw/hack_rf.cpp
    src/hw/pluto_sdr.cpp
    src/hw/audio_sink.cpp
    src/hw/alsa_sink.cpp
    src/config/json_config_loader.cpp
    src/dsp/amplitude_monitor.cpp
    src/dsp/equiripple_filter.cpp
    src/dsp/iq_converter.cpp
    src/dsp/quadrature_demodulator.cpp
    src/dsp/rc_deemphasis_filter.cpp
    src/dsp/audio_resampler.cpp
)

target_include_directories(wfm_hw
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
    PRIVATE
        ${HACKRF_INCLUDE_DIRS}
        ${IIO_INCLUDE_DIR}
        ${ALSA_INCLUDE_DIRS}
)

target_link_libraries(wfm_hw
    PUBLIC
        ${HACKRF_LIBRARIES}
        ${IIO_LIBRARY}
        ${ALSA_LIBRARIES}
        Boost::json
        Threads::Threads
)

target_compile_options(wfm_hw PRIVATE ${HACKRF_CFLAGS_OTHER} ${ALSA_CFLAGS_OTHER})

add_executable(wfm_receiver_demo src/app/main.cpp)
target_link_libraries(wfm_receiver_demo PRIVATE wfm_hw Boost::program_options)
