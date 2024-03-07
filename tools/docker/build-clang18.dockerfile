#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

FROM ubuntu:23.10

RUN \
    apt-get update && \
    apt-get --no-install-recommends -y install \
        wget \
        ca-certificates \
        libssl-dev \
        git \
        ninja-build \
        python3 \
        python3-requests \
        python-is-python3 \
        mysql-client && \
    echo 'deb http://apt.llvm.org/mantic/ llvm-toolchain-mantic-18 main' > /etc/apt/sources.list.d/llvm.list && \
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key > /etc/apt/trusted.gpg.d/apt.llvm.org.asc && \
    apt-get update && \
    apt-get --no-install-recommends -y install \
        clang-18 \
        libclang-rt-18-dev \
        libc++-18-dev \
        libc++abi-18-dev && \
    apt-get -y remove wget && \
    apt-get -y autoremove && \
    ln -s /usr/bin/clang++-18 /usr/bin/clang++ && \
    ln -s /usr/bin/clang-18 /usr/bin/clang

