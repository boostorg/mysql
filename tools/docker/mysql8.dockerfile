#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

FROM mysql:8.4.1

ENV MYSQL_ALLOW_EMPTY_PASSWORD=1
ENV MYSQL_ROOT_PASSWORD=

COPY tools/docker/ssl.cnf /etc/mysql/conf.d/
COPY tools/ssl/*.pem /etc/ssl/certs/mysql/

# Custom entry point to correctly set UNIX socket permissions, even if using volumes.
# Re-activate the native password plugin, which is now disabled by default
RUN <<EOF cat > /mysql_entrypoint.sh
chown -R mysql:mysql /var/run/mysqld
/bin/bash /usr/local/bin/docker-entrypoint.sh mysqld --mysql-native-password=ON
EOF

ENTRYPOINT ["/bin/bash", "/mysql_entrypoint.sh"]
