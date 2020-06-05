FROM mysql:8.0.20

ENV MYSQL_ALLOW_EMPTY_PASSWORD=1
ENV MYSQL_ROOT_PASSWORD=

COPY example/db_setup.sql /docker-entrypoint-initdb.d/example_db_setup.sql
COPY test/integration/*.sql /docker-entrypoint-initdb.d/
COPY tools/mysql_entrypoint.sh /

ENTRYPOINT ["/bin/bash", "/mysql_entrypoint.sh"]
