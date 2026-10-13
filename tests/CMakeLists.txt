include(FetchContent)

FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

file(GLOB_RECURSE TEST_SOURCES
  CONFIGURE_DEPENDS
  "*.cpp"
)

add_executable(wavelet_tests ${TEST_SOURCES})

target_include_directories(wavelet_tests
  PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(wavelet_tests
  PRIVATE
    wavelet_analysis_lib
    Catch2::Catch2WithMain
)

target_compile_options(wavelet_tests
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-O0 -g>
)

include(CTest)
include(Catch)
catch_discover_tests(wavelet_tests)
