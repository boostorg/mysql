#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

# Config
export SOURCE_DIR=$(pwd)
export BOOST_ROOT="$HOME/boost-root"
export PATH="$BOOST_ROOT:$PATH"

# Get Boost
./tools/install_boost.sh
cd $BOOST_ROOT

# Build
./b2 \
    toolset=$B2_TOOLSET \
    variant=$B2_VARIANT \
    cxxstd=$B2_CXXSTD \
    stdlib=$B2_STDLIB \
    -j4 \
    libs/mysql/test \
    libs/mysql/test//boost_mysql_integrationtests \
    libs/mysql/example//boost_mysql_all_examples
