@echo off

rem Download submodules.
if not exist libs\wxWidgets\README.md (
    echo "Initialising submodules..."
    git submodule update --init --recursive
)

mkdir out
cd out
cmake ..
cmake --build .