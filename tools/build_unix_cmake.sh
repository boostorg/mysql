#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# cwd should be our repo
# BUILD_SHARED_LIBS: required
# USE_VALGRIND: optional, to enable valgrind
# USE_COVERAGE: optional, to enable coverage
# BOOST_MYSQL_DATABASE: optional, to disable sha256 tests

set -e

# Config
export SOURCE_DIR=$(pwd)
export BOOST_ROOT="$HOME/boost-root"
export PATH="$BOOST_ROOT:$PATH"
if [ "$BOOST_MYSQL_DATABASE" != "mysql8" ]; then
    export BOOST_MYSQL_NO_SHA256_TESTS=1
fi
BOOST_B2_DISTRO=$HOME/.local/boost-b2-distro
BOOST_CMAKE_DISTRO=$HOME/.local/boost-cmake-distro
CMAKE_CXX_STANDARD=20
CMAKE_BUILD_TYPE=Debug
CMAKE_TEST_FOLDER="$BOOST_ROOT/libs/mysql/test/cmake_test"
CMAKE_GENERATOR=Ninja
export LD_LIBRARY_PATH=$BOOST_B2_DISTRO/lib:$LD_LIBRARY_PATH

# Get Boost
./tools/install_boost.sh
cd "$BOOST_ROOT"
./b2 \
    --prefix=$BOOST_B2_DISTRO \
    --with-system \
    --with-context \
    --with-coroutine \
    --with-date_time \
    --with-test \
    -d0 \
    cxxstd=$CMAKE_CXX_STANDARD \
    install

# Library tests, run from the superproject
cd "$BOOST_ROOT"
mkdir -p __build_cmake_test__ && cd __build_cmake_test__
cmake \
    -G "$CMAKE_GENERATOR" \
    -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
    -DCMAKE_CXX_STANDARD=$CMAKE_CXX_STANDARD \
    -DBOOST_INCLUDE_LIBRARIES=mysql \
    -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS \
    -DBUILD_TESTING=ON \
    -DBoost_VERBOSE=ON \
    ..
cmake --build . --target tests --config $CMAKE_BUILD_TYPE -j4
ctest --output-on-failure --build-config $CMAKE_BUILD_TYPE

# Library tests, using the b2 Boost distribution generated before
cd "$BOOST_ROOT/libs/mysql"
mkdir -p __build_standalone__ && cd __build_standalone__
cmake \
    -DCMAKE_PREFIX_PATH="$BOOST_B2_DISTRO" \
    -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
    -DCMAKE_CXX_STANDARD=$CMAKE_CXX_STANDARD \
    -DBOOST_MYSQL_INTEGRATION_TESTS=ON \
    $(if [ "$USE_VALGRIND" == 1 ]; then echo -DBOOST_MYSQL_VALGRIND_TESTS=ON; fi) \
    $(if [ "$USE_COVERAGE" == 1 ]; then echo -DBOOST_MYSQL_COVERAGE=ON; fi) \
    -G "$CMAKE_GENERATOR" \
    ..
cmake --build . -j 4
ctest --output-on-failure --build-config $CMAKE_BUILD_TYPE

# Subdir tests, using add_subdirectory()
cd "$CMAKE_TEST_FOLDER"
mkdir -p __build_cmake_subdir_test__ && cd __build_cmake_subdir_test__
cmake \
    -G "$CMAKE_GENERATOR" \
    -DBOOST_CI_INSTALL_TEST=OFF \
    -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
    -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS \
    ..
cmake --build . --config $CMAKE_BUILD_TYPE -j4
ctest --output-on-failure --build-config $CMAKE_BUILD_TYPE

# Install the library
cd "$BOOST_ROOT"
mkdir -p __build_cmake_install_test__ && cd __build_cmake_install_test__
cmake \
    -G "$CMAKE_GENERATOR" \
    -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
    -DBOOST_INCLUDE_LIBRARIES=mysql \
    -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS \
    -DCMAKE_INSTALL_PREFIX="$BOOST_CMAKE_DISTRO" \
    -DBoost_VERBOSE=ON \
    -DBoost_DEBUG=ON \
    ..
cmake --build . --target install --config $CMAKE_BUILD_TYPE -j4

# TODO: re-enable this when we get openssl support in the superproject generated install files 
# cd "$CMAKE_TEST_FOLDER"
# mkdir __build_cmake_install_test__ && cd __build_cmake_install_test__
# cmake \
#     -G "$CMAKE_GENERATOR" \
#     -DBOOST_CI_INSTALL_TEST=ON \
#     -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
#     -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS \
#     -DCMAKE_PREFIX_PATH="$BOOST_CMAKE_DISTRO" \
#     ..
# cmake --build . --config $CMAKE_BUILD_TYPE -j4
# ctest --output-on-failure --build-config $CMAKE_BUILD_TYPE


# Gather coverage data, if available
if [ "$USE_COVERAGE" == 1 ]; then
    cd "$BOOST_ROOT/libs/mysql"

    # Generate an adequate coverage.info file to upload. Codecov's
    # default is to compute coverage for tests and examples, too, which
    # is not correct
    lcov --capture --directory . -o coverage.info
    lcov -o coverage.info --extract coverage.info "**include/boost/mysql/**"
    
    # Upload the results
    codecov -Z -f coverage.info
fi
