#!/bin/bash

set -e

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

mysql -u root < $SCRIPTPATH/db_setup.sql
./mysql_integrationtests