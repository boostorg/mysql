#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

B2_VARIANT=debug
B2_CXXSTD=20
B2_STDLIB=libc++
B2_TOOLSET=clang-12

export BOOST_MYSQL_SERVER_HOST=mysql
export BOOST_ROOT=$HOME/boost-root
export PATH="$BOOST_ROOT:$PATH"

get_boost () {
    # Clone Boost
    git clone -b master --depth 1 https://github.com/boostorg/boost.git $BOOST_ROOT
    cd $BOOST_ROOT

    # Put our library inside boost root
    mkdir -p $BOOST_ROOT/libs
    ln -s /opt/boost-mysql $BOOST_ROOT/libs/mysql

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
}

if [ -e $BOOST_ROOT ]; then
    cd $BOOST_ROOT
    cp $BOOST_ROOT/libs/mysql/tools/user-config.jam ~/user-config.jam
else
    get_boost
fi

./b2 \
    variant=$B2_VARIANT \
    cxxstd=$B2_CXXSTD \
    stdlib=$B2_STDLIB \
    toolset=$B2_TOOLSET \
    -j8 \
    libs/mysql/test \
    libs/mysql/test//boost_mysql_integrationtests \
    libs/mysql/example//boost_mysql_all_examples
