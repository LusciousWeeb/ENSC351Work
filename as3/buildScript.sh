#!/bin/bash
# Cross-compile beatbox for the BeagleY-AI.
# The CMake POST_BUILD step deploys the binary, WAV files and Node server
# to ~/ensc351/public/myApps/.
rm -rf build/
cmake -S . -B build/target -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc
cmake --build build/target
