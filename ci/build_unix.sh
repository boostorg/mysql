#!/bin/bash

# SSL stuff
sudo cp ci/unix-ssl.cnf /etc/mysql/conf.d/ssl.cnf
cp ci/*.pem /tmp
sudo service mysql restart

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE $CMAKE_OPTIONS .. 
make -j6 CTEST_OUTPUT_ON_FAILURE=1 all test