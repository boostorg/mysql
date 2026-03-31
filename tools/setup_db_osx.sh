#!/bin/bash
#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Setup database. We can't use a Docker service as Docker
# is not supported in osx.

# Install the DB
brew install mysql@8.0
export PATH="/opt/homebrew/opt/mysql@8.0/bin:$PATH"

# Generate the certificates
mkdir -p /tmp/mysql-tls
python tools/ci/gen-certificates.py /tmp/mysql-tls

# Copy config files and set up paths
cp tools/osx-ci.cnf ~/.my.cnf
sudo mkdir -p /var/run/mysqld/
sudo chmod 777 /var/run/mysqld/

# Start the server
brew services start mysql@8.0
