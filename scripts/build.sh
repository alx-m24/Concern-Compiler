#!/usr/bin/env bash
set -e

emcmake cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
