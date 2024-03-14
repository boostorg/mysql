#!/bin/bash
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

BK=b2
IMAGE=build-gcc13
SHA=252732b3d7af7f78618e877479b85d4d611a61f4
CONTAINER=builder-$IMAGE-$BK
FULL_IMAGE=ghcr.io/anarthal-containers/$IMAGE:$SHA
DB=mysql8

docker start $DB || docker run -d \
    --name $DB \
    -v /var/run/mysqld:/var/run/mysqld \
    -p 3306:3306 \
    ghcr.io/anarthal-containers/$DB:$SHA
docker start $CONTAINER || docker run -dit \
    --name $CONTAINER \
    -v ~/workspace/mysql:/opt/boost-mysql \
    -v /var/run/mysqld:/var/run/mysqld \
    $FULL_IMAGE
docker network connect my-net $DB || echo "DB already connected"
docker network connect my-net $CONTAINER || echo "Network already connected"
docker exec $CONTAINER python /opt/boost-mysql/tools/ci/main.py --source-dir=/opt/boost-mysql \
    --build-kind=$BK \
    --build-shared-libs=1 \
    --valgrind=0 \
    --coverage=0 \
    --clean=0 \
    --toolset=gcc \
    --address-model=64 \
    --address-sanitizer=0 \
    --undefined-sanitizer=0 \
    --cxxstd=20 \
    --variant=debug \
    --separate-compilation=1 \
    --use-ts-executor=0 \
    --cmake-standalone-tests=1 \
    --cmake-add-subdir-tests=1 \
    --cmake-install-tests=1 \
    --cmake-build-type=Debug \
    --coverage=0 \
    --stdlib=native \
    --server-host=$DB \
    --db=$DB

if [ "$BK" == "docs" ]; then
    cp -r ~/workspace/mysql/doc/html ~/workspace/boost-root/libs/mysql/doc/
fi
