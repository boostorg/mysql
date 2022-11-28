#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# BOOST_ROOT: required, directory where boost should be located
# SOURCE_DIR: required, path to our library
# USE_SYMLINK: optional, whether to copy mysql or use symlinks

set -e

if [ ! -d $BOOST_ROOT ]; then

    # Clone Boost
    git clone -b master --depth 1 https://github.com/boostorg/boost.git $BOOST_ROOT
    cd $BOOST_ROOT

    # Put our library inside boost root. Symlinks are used during local development
    if [ "$USE_SYMLINK" == "1" ]; then
        ln -s $SOURCE_DIR $BOOST_ROOT/libs/mysql
    else
        mkdir -p libs/mysql
        cp -rf $SOURCE_DIR/* $BOOST_ROOT/libs/mysql/
    fi

    # Install Boost dependencies
    git config submodule.fetchJobs 8 # If supported, this will accelerate depinst
    git submodule update -q --init tools/boostdep
    python tools/boostdep/depinst/depinst.py --include example mysql

    # Booststrap
    ./bootstrap.sh
    ./b2 headers

fi
