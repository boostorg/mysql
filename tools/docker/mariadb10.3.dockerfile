FROM mariadb:10.3

ENV MYSQL_ALLOW_EMPTY_PASSWORD=1
ENV MYSQL_ROOT_PASSWORD=

COPY example/db_setup.sql /docker-entrypoint-initdb.d/example_db_setup.sql
COPY test/integration/db_setup.sql /docker-entrypoint-initdb.d/
COPY tools/docker/mysql_entrypoint.sh /
COPY tools/docker/*.pem /etc/ssl/certs/mysql/
COPY tools/docker/mariadb10.3.ssl.cnf /etc/mysql/conf.d/

ENTRYPOINT ["/bin/bash", "/mysql_entrypoint.sh"]