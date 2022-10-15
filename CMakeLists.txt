cmake_minimum_required(VERSION 3.22)

# Use `ccache` if it is installed in system's PATH.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Export compilation database in JSON format.
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

project(CNOTE_PROJECT LANGUAGES CXX)

add_executable (
  CNOTE
  src/main.cpp
)
set_property(TARGET CNOTE PROPERTY CXX_STANDARD 20)

target_include_directories (
  CNOTE
  PUBLIC
  wxWidgets/wxMSW-3.2.1_gcc1030TDM_x64_Dev/lib/gcc1030TDM_x64_dll/mswud
  wxWidgets/wxWidgets-3.2.1-headers/include
)

target_link_directories (
  CNOTE
  PUBLIC
  wxWidgets/wxMSW-3.2.1_gcc1030TDM_x64_Dev/lib/gcc1030TDM_x64_dll
)

target_link_libraries (
  CNOTE
  PUBLIC
  wxmsw32ud_core
  wxbase32ud
  wxtiffd
  wxjpegd
  wxpngd
  wxzlibd
  wxregexud
  wxexpatd
)

target_compile_options (
  CNOTE
  PRIVATE
  -Wall
  -Wextra
  -pedantic
)
