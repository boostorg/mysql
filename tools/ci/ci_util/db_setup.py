#!/usr/bin/python3
#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from typing import List, Dict
from pathlib import Path
import subprocess
import os
import time
from .common import IS_WINDOWS
from enum import Enum
import re


# In CI, the DB server runs in a container that is launched in parallel to ours,
# so it may not be ready by the time we try to load the data into it
_SERVER_WAIT_TIMEOUT_SECONDS = 180
_SERVER_WAIT_INTERVAL_SECONDS = 2


class _DbSystemType(Enum):
    mysql5 = 1
    mysql8 = 2
    mysql9 = 3
    mariadb = 4


def _run_piped_stdin(args: List[str], fname: Path) -> None:
    with open(str(fname), 'rt', encoding='utf8') as f:
        content = f.read()
    print('+ ', args, '(with < {})'.format(fname), flush=True)
    subprocess.run(args, input=content.encode(), check=True)


def _run_piped_stdout(args: List[str]) -> str:
    print('+ ', args, flush=True)
    return subprocess.check_output(args).decode()


def _run_sql_file(fname: Path) -> None:
    _run_piped_stdin(['mysql', '-u', 'root'], fname)


def _is_server_ready(server_host: str) -> bool:
    # Force TCP: while the server is initializing, the official containers launch a
    # temporary server that is reachable through the UNIX socket, but not by TCP.
    # We're not interested in such a server
    res = subprocess.run(
        ['mysql', '-u', 'root', '-h', server_host, '--protocol=tcp', '-e', 'SELECT 1'],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    return res.returncode == 0


def _wait_for_server(server_host: str) -> None:
    print('+ Waiting for the DB server to become ready', flush=True)
    deadline = time.monotonic() + _SERVER_WAIT_TIMEOUT_SECONDS
    while not _is_server_ready(server_host):
        if time.monotonic() >= deadline:
            raise RuntimeError('The DB server was not ready after {} seconds'.format(
                _SERVER_WAIT_TIMEOUT_SECONDS
            ))
        time.sleep(_SERVER_WAIT_INTERVAL_SECONDS)
    print('+ The DB server is ready', flush=True)


def _get_server_version() -> str:
    return _run_piped_stdout([
        'mysql', '-u', 'root', '--column-names=0',  '--batch', '-e', 'SELECT VERSION()'
    ])


def _parse_db_version(db: str) -> _DbSystemType:
    # Parse the version string
    match = re.match(r'([0-9]*)\.[0-9]*\.[0-9]*(\-MariaDB)?.*', db, re.IGNORECASE)
    if match is None:
        raise ValueError('Bad DB version: {}'.format(db))
    vmaj = int(match.group(1))
    is_mariadb = match.group(2) is not None

    # Perform the matching
    if is_mariadb:
        return _DbSystemType.mariadb
    elif vmaj == 8:
        return _DbSystemType.mysql8
    elif vmaj >= 9:
        return _DbSystemType.mysql9
    else:
        return _DbSystemType.mysql5


def _compute_disabled_features(db: _DbSystemType) -> Dict[str, bool]:
    return {
        # UNIX sockets. Only Windows CI servers don't have them enabled
        'unix-sockets': IS_WINDOWS,

        # sha256. Disabled on mysql5 and mariadb
        'sha256': db in (_DbSystemType.mysql5, _DbSystemType.mariadb),

        # JSON type. Disabled in mariadb
        'json-type': db == _DbSystemType.mariadb,

        # regex-error-codes. Disabled in mysql5 and mariadb
        'regex-error-codes': db in (_DbSystemType.mysql5, _DbSystemType.mariadb),

        # dup-query-error-codes. Disabled in mysql systems
        'dup-query-error-codes': db in (_DbSystemType.mysql5, _DbSystemType.mysql8, _DbSystemType.mysql9),

        # mysql_native_password. Disabled in mysql9
        'mnp': db == _DbSystemType.mysql9,
    }


def db_setup(
    source_dir: Path,
    server_host: str,
) -> None:
    # The server may still be starting up
    _wait_for_server(server_host)

    # Get the server version
    db = _get_server_version()
    print('+ Server version: {}'.format(db), flush=True)

    # Get the disabled server features
    disabled_features = _compute_disabled_features(_parse_db_version(db))
    disabled_features_str = ' '.join(feature for feature, disabled in disabled_features.items() if disabled)
    print('+ Disabled server features: {}'.format(disabled_features_str))

    # Source files
    _run_sql_file(source_dir.joinpath('example', 'db_setup.sql'))
    _run_sql_file(source_dir.joinpath('example', '3_advanced', 'http_server_cpp20', 'db_setup.sql'))
    _run_sql_file(source_dir.joinpath('test', 'integration', 'db_setup.sql'))
    if not disabled_features['sha256']:
        _run_sql_file(source_dir.joinpath('test', 'integration', 'db_setup_sha256.sql'))
    if not disabled_features['mnp']:
        _run_sql_file(source_dir.joinpath('test', 'integration', 'db_setup_mnp.sql'))
    
    # Setup environment variables
    os.environ['BOOST_MYSQL_SERVER_HOST'] = server_host
    os.environ['BOOST_MYSQL_DISABLED_SERVER_FEATURES'] = disabled_features_str


# Loads benchmark data. This takes around 15s, so we don't do it unless necessary
def db_setup_bench(source_dir: Path, server_host: str) -> None:
    _wait_for_server(server_host)
    _run_sql_file(source_dir.joinpath('bench', 'db_setup.sql'))
