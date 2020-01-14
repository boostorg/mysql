#!/bin/bash

set -e

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

mysql -u root < $SCRIPTPATH/db_setup.sql
./example_query_sync  example_user example_password
./example_query_async example_user example_password
./example_metadata    example_user example_password