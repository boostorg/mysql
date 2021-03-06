#!/bin/bash
#
# Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

SELF_PATH=$HOME/workspace/mysql

git clone --depth 1 https://github.com/boostorg/boost.git boost-root
cd boost-root
git submodule update -q --init tools/boostdep
ln -s $SELF_PATH $(pwd)/libs/mysql
ln -s $SELF_PATH/tools/user-config.jam $HOME/
export BOOST_ROOT="$(pwd)"
export PATH="$(pwd):$PATH"

python tools/boostdep/depinst/depinst.py --include benchmark --include example --include examples --include tools mysql
./bootstrap.sh
./b2 headers
