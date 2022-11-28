#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

export BUILD_SHARED_LIBS=OFF
export USE_VALGRIND=0
export USE_COVERAGE=0
export BOOST_MYSQL_DATABASE=mysql8
export BOOST_MYSQL_SERVER_HOST=mysql
export USE_SYMLINK=0 # subdir tests don't like symlinks

# The CI script expect us to be in the source directory
cd /opt/boost-mysql

# rm -rf ~/boost-root
./tools/build_unix_cmake.sh
