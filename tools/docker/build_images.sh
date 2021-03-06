#!/bin/bash
#
# Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Builds and pushes all docker images.
# This is NOT run by any CI

set -e

images=(mariadb:10.3 mysql:5 mysql:8)

cd "$(dirname "$0")/../.."

for img in "${images[@]}"
do
    echo "Building $img"
    docker build -t anarthal/$img -f "tools/docker/${img/:/}.dockerfile" .
    docker push anarthal/$img
done

