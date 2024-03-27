#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from pathlib import Path
import os
from typing import List, Optional
from .common import run, IS_WINDOWS
from .db_setup import db_setup
from .install_boost import install_boost


def _conditional_run(args: List[Optional[str]]) -> None:
    run([elm for elm in args if elm is not None])


def _conditional(arg: str, condition: bool) -> Optional[str]:
    return arg if condition else None


def _win_openssl_line(address_model: int) -> str:
    s = 'using openssl : : <include>"C:/openssl-{0}/include" <search>"C:/openssl-{0}/lib" <ssl-name>libssl <crypto-name>libcrypto : <address-model>{0} ;\n'
    return s.format(address_model)


def _write_windows_user_config() -> None:
    config_path = str(Path.home().joinpath('user-config.jam'))
    print(' + Writing user-config.jam to {}'.format(config_path))
    with open(config_path, 'wt', encoding='utf-8') as f:
        for am in (32, 64):
            f.write(_win_openssl_line(am))


def b2_build(
    source_dir: Path,
    toolset: str,
    cxxstd: str,
    variant: str,
    stdlib: str,
    address_model: str,
    boost_branch: str,
    db: str,
    server_host: str,
    separate_compilation: bool,
    address_sanitizer: bool,
    undefined_sanitizer: bool,
    use_ts_executor: bool,
    coverage: bool,
    valgrind: bool,
    fail_if_no_openssl: bool,
) -> None:
    # Config
    os.environ['UBSAN_OPTIONS'] = 'print_stacktrace=1'
    if IS_WINDOWS:
        _write_windows_user_config()

    # Get Boost. This leaves us inside boost root
    install_boost(
        source_dir=source_dir,
        boost_branch=boost_branch,
    )

    # Setup DB
    db_setup(source_dir, db, server_host)

    # Invoke b2
    _conditional_run([
        'b2',
        '--abbreviate-paths',
        'toolset={}'.format(toolset),
        'cxxstd={}'.format(cxxstd),
        'address-model={}'.format(address_model),
        'variant={}'.format(variant),
        'stdlib={}'.format(stdlib),
        'boost.mysql.separate-compilation={}'.format('on' if separate_compilation else 'off'),
        'boost.mysql.use-ts-executor={}'.format('on' if use_ts_executor else 'off'),
        _conditional('address-sanitizer=norecover', address_sanitizer),
        _conditional('undefined-sanitizer=norecover', undefined_sanitizer),
        _conditional('coverage=on', coverage),
        _conditional('valgrind=on', valgrind),
        # Workaround for https://github.com/bfgroup/b2/issues/368
        _conditional('architecture=x86', address_model == '32' and not IS_WINDOWS),
        'warnings=extra',
        'warnings-as-errors=on',
        '-j4',
        'libs/mysql/test',
        'libs/mysql/test/integration//boost_mysql_integrationtests',
        'libs/mysql/test/thread_safety',
        'libs/mysql/example',
        _conditional('libs/mysql/test//fail_if_no_openssl', fail_if_no_openssl)
    ])
