#!/bin/bash
#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

BK=b2
IMAGE=build-gcc13
SHA=6efce57d289bfa028da8d4a9b50158f1f073984f
CONTAINER=builder-$IMAGE-$BK
FULL_IMAGE=ghcr.io/anarthal-containers/$IMAGE:$SHA
DB=mysql8

docker start $DB
docker start $CONTAINER || docker run -dit \
    --name $CONTAINER \
    -v ~/workspace/mysql:/opt/boost-mysql \
    -v /var/run/mysqld:/var/run/mysqld \
    $FULL_IMAGE
docker network connect my-net $CONTAINER || echo "Network already connected"
docker exec $CONTAINER python /opt/boost-mysql/tools/ci.py --source-dir=/opt/boost-mysql \
    --build-kind=$BK \
    --build-shared-libs=1 \
    --valgrind=0 \
    --coverage=0 \
    --clean=0 \
    --toolset=gcc-13 \
    --address-model=64 \
    --cxxstd=20 \
    --variant=debug \
    --separate-compilation=1 \
    --cmake-standalone-tests=1 \
    --cmake-add-subdir-tests=1 \
    --cmake-install-tests=1 \
    --cmake-build-type=Debug \
    --stdlib=native \
    --server-host=$DB \
    --db=$DB

if [ "$BK" == "docs" ]; then
    cp -r ~/workspace/mysql/doc/html ~/workspace/boost-root/libs/mysql/doc/
fi
