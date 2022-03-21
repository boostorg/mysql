#!/bin/bash
#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

docker start docbuilder || docker run -dit --name docbuilder -v ~/workspace/mysql:/opt/boost/libs/mysql anarthal/build-docs
docker exec docbuilder /opt/boost/libs/mysql/tools/scripts/build_docs_local_docker.sh
