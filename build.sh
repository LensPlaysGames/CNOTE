#!/usr/bin/env bash

# Download submodules
if ! test -f ./libs/wxWidgets/README.md; then
    echo "Initialising submodules..."
    git submodule update --init --recursive
fi

mkdir -p ./out
cd ./out
cmake ..
cmake --build .