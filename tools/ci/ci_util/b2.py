#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from pathlib import Path
import os
from .common import IS_WINDOWS, run
from .db_setup import db_setup
from .install_boost import install_boost

def b2_build(
    source_dir: Path,
    toolset: str,
    cxxstd: str,
    variant: str,
    stdlib: str,
    address_model: str,
    clean: bool,
    boost_branch: str,
    db: str,
    server_host: str,
    separate_compilation: bool,
    address_sanitizer: bool,
    undefined_sanitizer: bool,
    use_ts_executor: bool,
    coverage: bool,
    valgrind: bool,
) -> None:
    # Config
    os.environ['UBSAN_OPTIONS'] = 'print_stacktrace=1'
    if IS_WINDOWS:
        os.environ['OPENSSL_ROOT'] = 'C:\\openssl-{}'.format(address_model)

    # Get Boost. This leaves us inside boost root
    install_boost(
        source_dir=source_dir,
        boost_branch=boost_branch,
        clean=clean,
    )

    # Setup DB
    db_setup(source_dir, db, server_host)

    # Invoke b2
    run([
        'b2',
        '--abbreviate-paths',
        'toolset={}'.format(toolset),
        'cxxstd={}'.format(cxxstd),
        'address-model={}'.format(address_model),
        'variant={}'.format(variant),
        'stdlib={}'.format(stdlib),
        'boost.mysql.separate-compilation={}'.format('on' if separate_compilation else 'off'),
        'boost.mysql.use-ts-executor={}'.format('on' if use_ts_executor else 'off'),
    ] + (['address-sanitizer=norecover'] if address_sanitizer else [])     # can only be disabled by omitting the arg
      + (['undefined-sanitizer=norecover'] if undefined_sanitizer else []) # can only be disabled by omitting the arg
      + (['coverage=on'] if coverage else [])
      + (['valgrind=on'] if valgrind else [])
      + [
        'warnings=extra',
        'warnings-as-errors=on',
        '-j4',
        'libs/mysql/test',
        'libs/mysql/test/integration//boost_mysql_integrationtests',
        'libs/mysql/test/thread_safety',
        'libs/mysql/example'
    ])