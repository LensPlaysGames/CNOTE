#!/usr/bin/env bash

# Download submodules
if ! test -e ./libs/wxWidgets/include; then
    echo "Initialising submodules..."
    git submodule update --init --recursive
fi

cmake -B out
cmake --build out
