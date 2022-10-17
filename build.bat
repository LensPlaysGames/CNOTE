rem Download submodules.
if not exist libs\wxWidgets\include (
    echo Initialising submodules...
    git submodule update --init --recursive
)

rem Build the project.
cmake -B out
cmake --build out
