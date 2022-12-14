cmake_minimum_required(VERSION 3.22)
project(CNOTE_PROJECT LANGUAGES CXX)

## ============================================================================
##  Global vars
## ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}")

## ============================================================================
##  CCache
## ============================================================================
# Use `ccache` if it is installed in system's PATH.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Export compilation database in JSON format.
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

## ============================================================================
##  Submodules
## ============================================================================
# Download submodules if we haven't done that already.
if (NOT EXISTS "${PROJECT_SOURCE_DIR}/libs/wxWidgets/README.md")
  execute_process(
    COMMAND git submodule update --init --recursive
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    #COMMENT "Initialising submodules. This WILL take a while."
    COMMAND_ERROR_IS_FATAL ANY
    ECHO_ERROR_VARIABLE
    ECHO_OUTPUT_VARIABLE
  )
endif()

# Don't use DLLs on Windows; this avoids the need to drag them around
# with the executable.
if (WIN32)
  set(wxBUILD_SHARED OFF)
endif()

add_subdirectory(libs/wxWidgets)

add_subdirectory(libs/fmt)

## ============================================================================
##  CNOTE
## ============================================================================
add_library (
  CNOTE_LIB
  src/cnote.cpp
)

target_include_directories (
  CNOTE_LIB
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/libs/fmt/include
)

target_link_libraries (
  CNOTE_LIB
  PRIVATE
  fmt
)

if (WIN32)
  add_executable (CNOTE WIN32 src/main.cpp)
else()
  add_executable (CNOTE src/main.cpp)
endif()

# wxWidget stuff
target_include_directories(CNOTE PRIVATE
  ${PROJECT_SOURCE_DIR}/libs/wxWidgets/include
  ${PROJECT_SOURCE_DIR}/libs/fmt/include
  ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries (CNOTE PRIVATE CNOTE_LIB fmt wx::net wx::core wx::base)

# Compile options
if (NOT MSVC)
  target_compile_options (
    CNOTE PRIVATE
    -Wall
    -Wextra
    -pedantic
  )
else()
  target_compile_options(CNOTE PRIVATE /W3)
endif()
