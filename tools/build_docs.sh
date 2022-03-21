#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

MYSQL_LIB_DIR=$(pwd)

# Install the library
cp -r . /opt/boost/libs/mysql
cp tools/user-config.jam ~/

# Build docs
cd /opt/boost/
export BOOST_ROOT=$(pwd)
./b2 -j4 libs/mysql/doc//boostrelease
