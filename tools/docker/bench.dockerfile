#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

FROM ubuntu:24.04

RUN \
    apt-get update && \
    apt-get --no-install-recommends -y install \
        gcc-14 \
        g++-14 \
        libssl-dev \
        git \
        ca-certificates \
        python3 \
        python3-requests \
        python-is-python3 \
        cmake \
        ninja-build \
        gpg \
        wget \
        xz-utils \
        mariadb-client \
        libmariadb-dev \
        gpg-agent && \
    ln -s /usr/bin/g++-14 /usr/bin/g++ && \
    ln -s /usr/bin/gcc-14 /usr/bin/gcc

# Install libmysqlclient. This includes many directories we don't care about.
# Removing them reduces the image size.
# Includes in this tarfile are placed directly under include/,
# unlike the ones in deb packages. Move them to reproduce deb layout.
RUN wget -q https://dev.mysql.com/get/Downloads/MySQL-8.4/mysql-8.4.4-linux-glibc2.28-x86_64.tar.xz && \
    tar -xf mysql-8.4.4-linux-glibc2.28-x86_64.tar.xz && \
    mkdir -p /opt/mysql-8.4.4/include/mysql && \
    mv mysql-8.4.4-linux-glibc2.28-x86_64/lib /opt/mysql-8.4.4 && \
    mv mysql-8.4.4-linux-glibc2.28-x86_64/include/* /opt/mysql-8.4.4/include/mysql/ && \
    rm mysql-8.4.4-linux-glibc2.28-x86_64.tar.xz && \
    rm -rf mysql-8.4.4-linux-glibc2.28-x86_64
