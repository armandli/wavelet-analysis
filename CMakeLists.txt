cmake_minimum_required(VERSION 3.28)

project(wavelet_analysis
  VERSION 0.1.0
  DESCRIPTION "Wavelet-based text structure analysis CLI"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

include(FetchContent)

find_package(CLI11 QUIET CONFIG)
if(NOT CLI11_FOUND)
  FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
  )
  FetchContent_MakeAvailable(CLI11)
endif()

file(GLOB_RECURSE SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(wavelet_analysis ${SOURCES})

target_include_directories(wavelet_analysis
  PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(wavelet_analysis
  PRIVATE
    CLI11::CLI11
)

target_compile_options(wavelet_analysis
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
    $<$<CONFIG:Debug>:-O0 -g>
)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(BUILD_TESTS "Build unit tests" OFF)
  if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
  endif()
endif()
