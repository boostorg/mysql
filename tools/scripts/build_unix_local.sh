#!/bin/bash
#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

repo_base=$(realpath $(dirname $(realpath $0))/../..)

BK=docs
IMAGE=build-docs
IMAGE_VERSION=1
CONTAINER=builder-$IMAGE
FULL_IMAGE=ghcr.io/anarthal/cpp-ci-containers/$IMAGE:$IMAGE_VERSION

docker start $CONTAINER || docker run -dit \
    --name $CONTAINER \
    --network host \
    -v "$repo_base:/opt/boost-mysql" \
    -v /var/run/mysqld:/var/run/mysqld \
    $FULL_IMAGE

# Command line
case $BK in
    b2) cmd="--toolset=clang
            --cxxstd=11
            --variant=release
            --stdlib=native
            --address-model=64
            --separate-compilation=1
            --use-ts-executor=0
            --address-sanitizer=0
            --undefined-sanitizer=0
            --coverage=0
            --valgrind=0"
        ;;
    
    cmake) cmd="
            --cmake-build-type=Debug
            --build-shared-libs=1
            --cxxstd=11
            --install-test=0
            "
        ;;
    
    bench) cmd="
                --protocol-iters=10
                --connection-pool-iters=0
                "
        ;;

    *) cmd="" ;;
esac

# Run
docker exec $CONTAINER python /opt/boost-mysql/tools/ci/main.py --source-dir=/opt/boost-mysql $BK $cmd
