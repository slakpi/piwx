#!/bin/sh
export CC=gcc-8
export CXX=g++-8
cmake -DCMAKE_INSTALL_PREFIX=$(realpath ..) -DETC_PREFIX="./" -DCMAKE_BUILD_TYPE=Debug ..
