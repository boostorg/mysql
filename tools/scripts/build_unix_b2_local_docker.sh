#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

export B2_VARIANT=debug
export B2_CXXSTD=20
export B2_STDLIB=libc++
export B2_TOOLSET=clang-14
export BOOST_MYSQL_SERVER_HOST=mysql
export USE_SYMLINK=1

# The CI script expect us to be in the source directory
cd /opt/boost-mysql

# rm -rf ~/boost-root
./tools/build_unix_b2.sh
