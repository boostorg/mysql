#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

# Install CMake
mkdir /opt/cmake
chmod 777 /opt/cmake
wget https://github.com/Kitware/CMake/releases/download/v3.17.0/cmake-3.17.0-Linux-x86_64.sh -q -O cmake-latest.sh
bash cmake-latest.sh --prefix=/opt/cmake --skip-license
rm cmake-latest.sh

# Download and build Boost
wget https://boostorg.jfrog.io/artifactory/main/release/1.78.0/source/boost_1_78_0.tar.gz -q
tar -xf boost_1_78_0.tar.gz
cd boost_1_78_0
./bootstrap.sh --prefix=/opt/boost
./b2 \
    --with-system \
    --with-context \
    --with-coroutine \
    --with-date_time \
    --with-test \
    -d0 \
    install
cd ..
rm -rf boost_1_78_0.tar.gz boost_1_78_0