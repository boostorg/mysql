#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from typing import List, Dict
from pathlib import Path
import subprocess
import os
from .common import IS_WINDOWS
from enum import Enum
import re


class _DbSystemType(Enum):
    mysql5 = 1
    mysql8 = 2
    mariadb = 3


def _parse_db_version(version: str) -> _DbSystemType:
    # Parse the version string
    match = re.match('([a-zA-Z0-9_]*)\\-([0-9]*)\\.[0-9]*\\.[0-9]*', version)
    if match is None:
        raise ValueError('Bad DB version: {} (should be: dbflavor-vmajor.vminor.vpatch)'.format(version))
    flavor = match.group(1)
    vmaj = int(match.group(2))

    # Perform the matching
    if flavor == 'mysql':
        return _DbSystemType.mysql8 if vmaj >= 8 else _DbSystemType.mysql5
    elif flavor == 'mariadb':
        return _DbSystemType.mariadb
    else:
        raise ValueError('Bad DB flavor: {} - full version: {}', flavor, version)


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
        'dup-query-error-codes': db in (_DbSystemType.mysql5, _DbSystemType.mysql8),
    }



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
    # Get the disabled server features
    disabled_features = _compute_disabled_features(_parse_db_version(db))
    disabled_features_str = ' '.join(feature for feature, disabled in disabled_features.items() if disabled)
    print(' + Disabled server features: {}'.format(disabled_features_str))

    # Source files
    _run_sql_file(source_dir.joinpath('example', 'db_setup.sql'))
    _run_sql_file(source_dir.joinpath('example', 'order_management', 'db_setup.sql'))
    _run_sql_file(source_dir.joinpath('test', 'integration', 'db_setup.sql'))
    if not disabled_features['sha256']:
        _run_sql_file(source_dir.joinpath('test', 'integration', 'db_setup_sha256.sql'))
    
    # Setup environment variables
    os.environ['BOOST_MYSQL_SERVER_HOST'] = server_host
    os.environ['BOOST_MYSQL_DISABLED_SERVER_FEATURES'] = disabled_features_str
