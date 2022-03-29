#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

# Config
SOURCE_DIR=$GITHUB_WORKSPACE

# Clone Boost
cd ~/
git clone -b master --depth 1 https://github.com/boostorg/boost.git boost-root
cd boost-root
export BOOST_ROOT=$(pwd)
export PATH="$(pwd):$PATH"

# Put our library inside boost root
mkdir -p libs/mysql
cp -rf $SOURCE_DIR/* $BOOST_ROOT/libs/mysql/

# Install Boost dependencies
git config submodule.fetchJobs 8 # If supported, this will accelerate depinst
git submodule update -q --init tools/boostdep
python tools/boostdep/depinst/depinst.py \
    --include benchmark \
    --include example \
    --include examples \
    --include tools \
    mysql


# Configure B2
cp $BOOST_ROOT/libs/mysql/tools/user-config.jam ~/user-config.jam

# Booststrap
./bootstrap.sh
./b2 headers

# Build
./b2 \
    variant=$B2_VARIANT \
    cxxstd=$B2_CXXSTD \
    stdlib=$B2_STDLIB \
    -j4 \
    libs/mysql/test \
    libs/mysql/test//boost_mysql_integrationtests \
    libs/mysql/example//boost_mysql_all_examples
