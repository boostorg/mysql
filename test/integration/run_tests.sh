#!/bin/bash

set -e

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

if [ "$MYSQL_SKIP_DB_SETUP" != "1" ]
then
	mysql -u root < $SCRIPTPATH/db_setup.sql
fi

./mysql_integrationtests