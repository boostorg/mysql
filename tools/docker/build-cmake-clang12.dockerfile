#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

FROM ubuntu:20.04

ENV PATH="/opt/cmake/bin:${PATH}"

COPY tools/docker/install_cmake_and_boost.sh .

RUN \
    apt-get update && \
    apt-get --no-install-recommends -y install \
        ca-certificates \
        clang-12 \
        libssl-dev \
        wget \
        python3 \
        ninja-build \
        valgrind && \
    ln -s /usr/bin/clang++-12 /usr/bin/clang++ && \
    ln -s /usr/bin/clang-12 /usr/bin/clang && \
    \
    bash install_cmake_and_boost.sh
