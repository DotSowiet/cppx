cmake_minimum_required(VERSION 3.22)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(TOML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TOML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TOML_BUILD_UTILITIES OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        10.1.1
)

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG        v3.4.0    
)

FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.5.0
)

FetchContent_MakeAvailable(fmt tomlplusplus CLI11)

find_package(CURL REQUIRED)

add_executable(cppx main.cpp
        helpers.cpp
)

target_link_libraries(cppx PRIVATE
    fmt::fmt
    tomlplusplus::tomlplusplus
    CLI11::CLI11
    CURL::libcurl
)
