rem Download submodules.
if not exist libs\wxWidgets\README.md (
    echo Initialising submodules...
    git submodule update --init --recursive
)

rem Build the project.
mkdir out
cd out
cmake ..
cmake --build .
cd ..