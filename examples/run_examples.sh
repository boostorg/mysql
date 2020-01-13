#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

mysql -u root < $SCRIPTPATH/db_setup.sql
./example_query_sync root ""
./example_query_async root ""
./example_metadata root ""