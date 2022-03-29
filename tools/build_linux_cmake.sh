#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

# Config
BOOST_ROOT=/opt/boost
export LD_LIBRARY_PATH=$BOOST_ROOT/lib:$LD_LIBRARY_PATH

if [ "$BOOST_MYSQL_DATABASE" != "mysql8" ]; then
    export BOOST_MYSQL_NO_SHA256_TESTS=1
fi

# Create the build directory
mkdir -p build
cd build

# Build the code
cmake \
    -DCMAKE_INSTALL_PREFIX=/tmp/boost_mysql \
    -DCMAKE_PREFIX_PATH=$BOOST_ROOT \
    -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
    -DCMAKE_CXX_STANDARD=$CMAKE_CXX_STANDARD \
    -DBOOST_MYSQL_INTEGRATION_TESTS=ON \
    $(if [ "$USE_VALGRIND" == 1 ]; then echo -DBOOST_MYSQL_VALGRIND_TESTS=ON; fi) \
    $(if [ "$USE_COVERAGE" == 1 ]; then echo -DBOOST_MYSQL_COVERAGE=ON; fi) \
    -G Ninja \
    ..

cmake --build . -j 4 --target install

# Run tests
ctest --verbose

# Gather coverage data, if available
if [ "$USE_COVERAGE" == 1 ]; then
    # Generate an adequate coverage.info file to upload. Codecov's
    # default is to compute coverage for tests and examples, too, which
    # is not correct
    lcov --capture --directory . -o coverage.info
    lcov -o coverage.info --extract coverage.info "**include/boost/mysql/**"
    
    # Upload the results
    codecov -Z -f coverage.info
fi

# Test that a user project could use our export
cd ..
python3 \
    tools/user_project_find_package/build.py \
    "-DCMAKE_PREFIX_PATH=/tmp/boost_mysql;$BOOST_ROOT"

