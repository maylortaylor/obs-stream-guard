# Builds the parts that need no OBS: unit tests and the file scan tool.
#   cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/core_tests
cmake_minimum_required(VERSION 3.28)
project(stream-guard-tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/deps.cmake)
find_package(CURL REQUIRED)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_library(
  sg_core
  STATIC
  ${SRC}/core/guard.cpp
  ${SRC}/core/track_check.cpp
  ${SRC}/core/audio_window.cpp
  ${SRC}/recognizer/shazam.cpp
)
target_include_directories(sg_core PUBLIC ${SRC})
target_link_libraries(sg_core PUBLIC sg_vibra nlohmann_json::nlohmann_json CURL::libcurl)
target_compile_options(sg_core PRIVATE -Wall -Wextra -Werror)

add_executable(core_tests core_tests.cpp)
target_link_libraries(core_tests PRIVATE sg_core)

add_executable(sg_scan ../tools/sg_scan.cpp)
target_link_libraries(sg_scan PRIVATE sg_core)

enable_testing()
add_test(NAME core COMMAND core_tests)
