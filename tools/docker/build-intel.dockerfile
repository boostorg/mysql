#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

FROM ubuntu:22.04

RUN \
    apt-get update && \
    apt-get --no-install-recommends -y install \
        ca-certificates \
        libssl-dev \
        wget \
        python3 \
        python-is-python3 \
        ninja-build \
        git \
        gpg \
        cmake \
        gpg-agent \
        mysql-client && \
    wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB \
        | gpg --dearmor | tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null && \
    echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" | tee /etc/apt/sources.list.d/oneAPI.list && \
    apt-get update && \
    apt-get --no-install-recommends -y install intel-oneapi-compiler-dpcpp-cpp-2024.0
