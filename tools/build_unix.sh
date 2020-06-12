#!/bin/bash
#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Get latest cmake
function get_cmake {
    if [ "$TRAVIS_OS_NAME" != "osx" ]; then # OSX cmake is good enough
        CMAKE_ROOT=/opt/cmake-latest
        sudo mkdir $CMAKE_ROOT
        sudo chmod 777 $CMAKE_ROOT
        wget https://github.com/Kitware/CMake/releases/download/v3.17.0/cmake-3.17.0-Linux-x86_64.sh -q -O cmake-latest.sh
        mkdir -p $CMAKE_ROOT
        bash cmake-latest.sh --prefix=$CMAKE_ROOT --skip-license
        export PATH=$CMAKE_ROOT/bin:$PATH
    fi
}

# Build latest boost (for CMake builds)
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

# Setup database
function setup_db {
    sudo mkdir -p /var/run/mysqld/
    sudo chmod 777 /var/run/mysqld/
    if [ "$TRAVIS_OS_NAME" == "osx" ]; then
        # No docker here. We just support MySQL 8
        if [ "$DATABASE" != "mysql:8" ]; then
            echo "Unsupported database on OSX: $DATABASE"
            exit 1
        fi
        cp tools/osx-ci.cnf ~/.my.cnf
        mysql.server start # Note that running this with sudo fails
        mysql -u root < test/integration/db_setup.sql
        mysql -u root < test/integration/db_setup_sha256.sql
        mysql -u root < example/db_setup.sql
    else
        sudo docker run \
            --name mysql \
            -v /var/run/mysqld:/var/run/mysqld \
            -p 3306:3306 \
            -d \
            anarthal/$DATABASE
    fi
    if [ "$DATABASE" == "mysql:8" ]; then
        export BOOST_MYSQL_SHA256_TESTS=1
    fi
}

# Gather coverage data and upload to codecov
function gather_coverage_data {
    # Select the gcov tool to use
    if [ "$TRAVIS_OS_NAME" == "osx" ]; then
        GCOV_TOOL="$TRAVIS_BUILD_DIR/tools/clang-gcov-osx.sh"
    elif [ "$TRAVIS_COMPILER" == "clang" ]; then
        GCOV_TOOL="$TRAVIS_BUILD_DIR/tools/clang-gcov-linux.sh"
    else
        GCOV_TOOL=gcov
    fi;

    cd build
    
    # Generate an adequate coverage.info file to upload. Codecov's
    # default is to compute coverage for tests and examples, too, which
    # is not correct
    lcov --capture --directory . -o coverage.info --gcov-tool "$GCOV_TOOL"
    lcov -o coverage.info --extract coverage.info "**include/boost/mysql/**"
    
    # Upload the results
    curl -s https://codecov.io/bash -o codecov.sh
    bash +x codecov.sh -f coverage.info
    
    cd ..
}

# Waits for DB to be ready
function wait_for_db {
    if [ "$TRAVIS_OS_NAME" != "osx" ]; then # No docker in OSX
        sudo python3 ../tools/wait_for_db_container.py  
    fi
}

# Runs the actual cmake build, test and install of our package
function cmake_build {
    # OSX requires setting OpenSSL root
    if [ "$TRAVIS_OS_NAME" == "osx" ]; then 
        local openssl_arg=-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
    fi
    
    # Actual CMake build
    mkdir -p build
    cd build
    cmake \
        -DCMAKE_INSTALL_PREFIX=/tmp/boost_mysql \
        "-DCMAKE_PREFIX_PATH=/tmp/date/;/opt/boost_1_73_0" \
        -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
        -DCMAKE_CXX_STANDARD=$CMAKE_CXX_STANDARD \
        $(if [ $USE_VALGRIND ]; then echo -DBOOST_MYSQL_VALGRIND_TESTS=ON; fi) \
        $(if [ $USE_COVERAGE ]; then echo -DBOOST_MYSQL_COVERAGE=ON; fi) \
        $(if [ $BOOST_MYSQL_SHA256_TESTS ]; then echo -DBOOST_MYSQL_SHA256_TESTS=ON; fi) \
        $openssl_arg \
        -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
        -DBOOST_MYSQL_ALLOW_FETCH_CONTENT=OFF \
        $CMAKE_OPTIONS \
        .. 
    make -j6 CTEST_OUTPUT_ON_FAILURE=1 all install
    wait_for_db
    ctest --output-on-failure
    cd ..
    
    # Test that a user project could use our export
    python3 \
        tools/user_project_find_package/build.py \
        "-DCMAKE_PREFIX_PATH=/tmp/boost_mysql;/tmp/date;/opt/boost_1_73_0" \
        $openssl_arg
}

# Main script
setup_db

if [ "$B2_TOOLSET" != "" ]; then # Boost.Build
    # Boost.Build setup
    cp tools/user-config.jam ~/user-config.jam
    
    # Dependencies
    export DATE_ROOT=/tmp/date/
    export GTEST_ROOT=/tmp/gtest/
    python3 tools/build_date.py $DATE_ROOT
    python3 tools/build_gtest.py $GTEST_ROOT
    # OSX requires setting OpenSSL root
    if [ "$TRAVIS_OS_NAME" == "osx" ]; then 
        export OPENSSL_ROOT=/usr/local/opt/openssl
    fi
    
    # Boost.CI install
    git clone https://github.com/boostorg/boost-ci.git boost-ci-cloned
    cp -prf boost-ci-cloned/ci .
    rm -rf boost-ci-cloned
    source ci/travis/install.sh
    
    # Boost.CI build. Note: takes time enough for DB container to load
    $BOOST_ROOT/libs/$SELF/ci/travis/build.sh
else # CMake
    get_cmake
    python3 tools/build_date.py /tmp/date/
    build_boost
    cmake_build
    if [ $USE_COVERAGE ]; then
        gather_coverage_data
    fi
fi        

