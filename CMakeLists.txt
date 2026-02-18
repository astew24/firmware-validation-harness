cmake_minimum_required(VERSION 3.16)
project(firmware_validation_harness LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(FVH_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(FVH_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

# Firmware: embedded-style C++. No exceptions, no heap, fixed-size buffers.
add_library(fw
  firmware/src/crc16.cpp
  firmware/src/packet.cpp
  firmware/src/sensor_controller.cpp)
target_include_directories(fw PUBLIC firmware/include)
target_compile_options(fw PRIVATE -fno-exceptions)

# Harness: simulated hardware, fault injection, telemetry decoding, validation.
add_library(harness
  harness/src/scenario.cpp
  harness/src/sim_hal.cpp
  harness/src/decoder.cpp
  harness/src/validator.cpp
  harness/src/baseline.cpp
  harness/src/metrics.cpp)
target_include_directories(harness PUBLIC harness/include)
target_link_libraries(harness PUBLIC fw)

find_package(Threads REQUIRED)
add_executable(fvh harness/src/main.cpp exporter/metrics_server.cpp)
target_include_directories(fvh PRIVATE exporter)
target_link_libraries(fvh PRIVATE harness Threads::Threads)

enable_testing()
add_executable(fvh_tests tests/test_main.cpp tests/test_firmware.cpp tests/test_harness.cpp)
target_link_libraries(fvh_tests PRIVATE harness)
add_test(NAME unit_tests COMMAND fvh_tests)
# Release gate: every scenario must pass and nothing may regress against the saved baseline.
add_test(NAME regression_gate
  COMMAND fvh --baseline ${CMAKE_SOURCE_DIR}/baseline/baseline.csv
              --results ${CMAKE_BINARY_DIR}/latest_results.csv)
