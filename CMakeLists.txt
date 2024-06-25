cmake_minimum_required(VERSION 3.22)
project(CNOTE LANGUAGES CXX)

## ============================================================================
##  Global vars
## ============================================================================
set(CMAKE_CXX_STANDARD 20)

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
##  CNOTE
## ============================================================================
add_library (
  CNOTE_LIB
  lib/cnote/cnote.cpp
  inc/cnote/cnote.h
)
target_include_directories (
  CNOTE_LIB
  PUBLIC
  inc
)

add_executable (
  CNOTE
  src/main.cpp
)
target_include_directories (
  CNOTE
  PRIVATE
  inc
)
target_link_libraries (
  CNOTE
  CNOTE_LIB
)
add_dependencies (
  CNOTE
  CNOTE_LIB
)

target_compile_options(CNOTE PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
