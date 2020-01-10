#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

mysql -u root -proot < $SCRIPTPATH/db_setup.sql
./example_query_sync root root