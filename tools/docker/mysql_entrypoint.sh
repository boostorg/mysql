#!/bin/bash

chown -R mysql:mysql /var/run/mysqld
/bin/bash /usr/local/bin/docker-entrypoint.sh mysqld --default_authentication_plugin=mysql_native_password