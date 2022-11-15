#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

MIRROR_LOCATION=~/workspace/boost-windows-mirror
BOOST_VERSION_MAJOR=1
BOOST_VERSION_MINOR=80
BOOST_VERSION_PATCH=0
BOOST_VERSION_DOT="${BOOST_VERSION_MAJOR}.${BOOST_VERSION_MINOR}.${BOOST_VERSION_PATCH}"
BOOST_VERSION_UND="boost_${BOOST_VERSION_MAJOR}_${BOOST_VERSION_MINOR}_${BOOST_VERSION_PATCH}"

BOOST_LINK_WINDOWS=https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION_DOT}/source/${BOOST_VERSION_UND}.zip

cd $MIRROR_LOCATION
rm -rf *
wget $BOOST_LINK_WINDOWS -O boost-latest-windows.zip
unzip -q boost-latest-windows.zip
rm boost-latest-windows.zip
rmdir $BOOST_VERSION_UND
git add .
git commit -m "Updated to version ${BOOST_VERSION_DOT}"
git push
