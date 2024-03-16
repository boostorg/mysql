#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from typing import List
from pathlib import Path
import subprocess
import os
from .common import IS_WINDOWS


def _run_piped_stdin(args: List[str], fname: Path) -> None:
    with open(str(fname), 'rt', encoding='utf8') as f:
        content = f.read()
    print('+ ', args, '(with < {})'.format(fname), flush=True)
    subprocess.run(args, input=content.encode(), check=True)


def _run_sql_file(fname: Path) -> None:
    _run_piped_stdin(['mysql', '-u', 'root'], fname)


def db_setup(
    source_dir: Path,
    db: str,
    server_host: str,
) -> None:
    # Source files
    _run_sql_file(source_dir.joinpath('example', 'db_setup.sql'))
    _run_sql_file(source_dir.joinpath('example', 'order_management', 'db_setup.sql'))
    _run_sql_file(source_dir.joinpath('test', 'integration', 'db_setup.sql'))
    if db == 'mysql8':
        _run_sql_file(source_dir.joinpath('test', 'integration', 'db_setup_sha256.sql'))
    
    # Setup environment variables
    os.environ['BOOST_MYSQL_SERVER_HOST'] = server_host
    os.environ['BOOST_MYSQL_TEST_DB'] = db
    if IS_WINDOWS:
        os.environ['BOOST_MYSQL_NO_UNIX_SOCKET_TESTS'] = '1'
