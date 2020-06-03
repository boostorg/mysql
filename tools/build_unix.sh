#!/bin/bash
#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Get latest cmake
function get_cmake {
    CMAKE_ROOT=/opt/cmake-latest
    sudo mkdir $CMAKE_ROOT
    sudo chmod 777 $CMAKE_ROOT
    wget https://github.com/Kitware/CMake/releases/download/v3.17.0/cmake-3.17.0-Linux-x86_64.sh -q -O cmake-latest.sh
    mkdir -p $CMAKE_ROOT
    bash cmake-latest.sh --prefix=$CMAKE_ROOT --skip-license
    export PATH=$CMAKE_ROOT/bin:$PATH
}

# Build latest boost
function build_boost {
    BOOST_ROOT=/opt/boost_1_73_0
    sudo mkdir $BOOST_ROOT
    sudo chmod 777 $BOOST_ROOT
    git clone https://github.com/anarthal/boost-unix-mirror.git boost-latest
    cd boost-latest
    ./bootstrap.sh --prefix=$BOOST_ROOT
    ./b2 --prefix=$BOOST_ROOT \
         --with-system \
         --with-context \
         --with-coroutine \
         --with-date_time \
         -d0 \
         toolset=$TRAVIS_COMPILER \
         install
    cd ..
    export LD_LIBRARY_PATH=$BOOST_ROOT/lib:$LD_LIBRARY_PATH
}

# SHA256 functionality is only supported in MySQL 8+. From our
# CI systems, only OSX has this version.

cp tools/ssl/*.pem /tmp # Copy SSL certs/keys to a known location
if [ $TRAVIS_OS_NAME == "osx" ]; then
    brew update
    brew install $DATABASE lcov ninja
    cp tools/unix-ci.cnf ~/.my.cnf  # This location is checked by both MySQL and MariaDB
    sudo mkdir -p /var/run/mysqld/
    sudo chmod 777 /var/run/mysqld/
    mysql.server start # Note that running this with sudo fails
    if [ $DATABASE == "mariadb" ]; then
        sudo mysql -u root < tools/root_user_setup.sql
    fi
    OPENSSL_ROOT_DIR_ARG=-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
else
    sudo cp tools/unix-ci.cnf /etc/mysql/conf.d/
    sudo service mysql restart
    get_cmake # OSX cmake is good enough
fi

build_boost
python3 tools/build_date.py /tmp/date/

mkdir -p build
cd build
cmake \
    -DCMAKE_INSTALL_PREFIX=/tmp/boost_mysql \
    -DCMAKE_PREFIX_PATH=/tmp/date/ \
    -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
    $(if [ $USE_VALGRIND ]; then echo -DBOOST_MYSQL_VALGRIND_TESTS=ON; fi) \
    $(if [ $USE_COVERAGE ]; then echo -DBOOST_MYSQL_COVERAGE=ON; fi) \
    $(if [ $HAS_SHA256 ]; then echo -DBOOST_MYSQL_SHA256_TESTS=ON; fi) \
    $OPENSSL_ROOT_DIR_ARG \
    -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
    -DBOOST_MYSQL_ALLOW_FETCH_CONTENT=OFF \
    $CMAKE_OPTIONS \
    .. 
make -j6 CTEST_OUTPUT_ON_FAILURE=1 all install test

# Coverage collection
if [ $USE_COVERAGE ]; then
    # Select the gcov tool to use
    if [ "$TRAVIS_OS_NAME" == "osx" ]; then
        GCOV_TOOL="$TRAVIS_BUILD_DIR/tools/clang-gcov-osx.sh"
    elif [ "$TRAVIS_COMPILER" == "clang" ]; then
        GCOV_TOOL="$TRAVIS_BUILD_DIR/tools/clang-gcov-linux.sh"
    else
        GCOV_TOOL=gcov
    fi;
    
    # Generate an adequate coverage.info file to upload. Codecov's
    # default is to compute coverage for tests and examples, too, which
    # is not correct
    lcov --capture --directory . -o coverage.info --gcov-tool "$GCOV_TOOL"
    lcov -o coverage.info --extract coverage.info "**include/boost/mysql/**"
    
    # Upload the results
    curl -s https://codecov.io/bash -o codecov.sh
    bash +x codecov.sh -f coverage.info
fi

# Test that a project using our cmake can do it
python3 ../tools/user_project_find_package/build.py \
    "-DCMAKE_PREFIX_PATH=/tmp/boost_mysql;/tmp/date" \
    $OPENSSL_ROOT_DIR_ARG
