#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from pathlib import Path
from typing import Union
import os
from shutil import rmtree, copytree
import argparse
from .common import IS_WINDOWS, BOOST_ROOT, install_boost, BoostInstallType, run, mkdir_and_cd
from .db_setup import db_setup
from .cmake import cmake_build, cmake_noopenssl_build, find_package_b2_test


def _add_to_path(path: Path) -> None:
    sep = ';' if IS_WINDOWS  else ':'
    os.environ['PATH'] = '{}{}{}'.format(path, sep, os.environ["PATH"])


def _doc_build(
    source_dir: Path,
    clean: bool,
    boost_branch: str
):
    # Get Boost. This leaves us inside boost root
    install_boost(
        BOOST_ROOT,
        source_dir=source_dir,
        clean=clean,
        install_type=BoostInstallType.docs,
        branch=boost_branch
    )

    # Write the config file
    config_path = os.path.expanduser('~/user-config.jam')
    with open(config_path, 'wt') as f:
        f.writelines(['using doxygen ;\n', 'using boostbook ;\n', 'using saxonhe ;\n'])

    # Run b2
    run(['b2', '-j4', 'cxxstd=17', 'libs/mysql/doc//boostrelease'])

    # Copy the resulting docs into a well-known path
    output_dir = source_dir.joinpath('doc', 'html')
    if output_dir.exists():
        rmtree(output_dir)
    copytree(BOOST_ROOT.joinpath('libs', 'mysql', 'doc', 'html'), output_dir)


def _b2_build(
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
        BOOST_ROOT,
        source_dir=source_dir,
        clean=clean,
        branch=boost_branch
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


def _str2bool(v: Union[bool, str]) -> bool:
    if isinstance(v, bool):
        return v
    elif v == '1':
        return True
    elif v == '0':
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')


def _deduce_boost_branch() -> str:
    # Are we in GitHub Actions?
    if os.environ.get('GITHUB_ACTIONS') is not None:
        ci = 'GitHub Actions'
        ref = os.environ.get('GITHUB_BASE_REF', '') or os.environ.get('GITHUB_REF', '')
        res = 'master' if ref == 'master' or ref.endswith('/master') else 'develop'
    elif os.environ.get('DRONE') is not None:
        ref = os.environ.get('DRONE_BRANCH', '')
        ci = 'Drone'
        res = 'master' if ref == 'master' else 'develop'
    else:
        ci = 'Unknown'
        ref = ''
        res = 'develop'
    
    print('+  Found CI {}, ref={}, deduced branch {}'.format(ci, ref, res))

    return res


def main():
    build_kinds = [
        'b2',
        'cmake',
        'cmake-noopenssl',
        'find-package-b2',
        'fuzz',
        'docs'
    ]

    parser = argparse.ArgumentParser()
    parser.add_argument('--build-kind', choices=build_kinds, required=True)
    parser.add_argument('--source-dir', type=Path, required=True)
    parser.add_argument('--boost-branch', default=None) # None means "let this script deduce it"
    parser.add_argument('--generator', default='Ninja')
    parser.add_argument('--build-shared-libs', type=_str2bool, default=True)
    parser.add_argument('--valgrind', type=_str2bool, default=False)
    parser.add_argument('--coverage', type=_str2bool, default=False)
    parser.add_argument('--clean', type=_str2bool, default=False)
    parser.add_argument('--db', choices=['mysql5', 'mysql8', 'mariadb'], default='mysql8')
    parser.add_argument('--cmake-build-type', choices=['Debug', 'Release', 'MinSizeRel'], default='Debug')
    parser.add_argument('--toolset', default='clang')
    parser.add_argument('--cxxstd', default='20')
    parser.add_argument('--variant', default='release')
    parser.add_argument('--stdlib', choices=['native', 'libc++'], default='native')
    parser.add_argument('--address-model', choices=['32', '64'], default='64')
    parser.add_argument('--separate-compilation', type=_str2bool, default=True)
    parser.add_argument('--use-ts-executor', type=_str2bool, default=False)
    parser.add_argument('--address-sanitizer', type=_str2bool, default=False)
    parser.add_argument('--undefined-sanitizer', type=_str2bool, default=False)
    parser.add_argument('--server-host', default='127.0.0.1')

    args = parser.parse_args()

    _add_to_path(BOOST_ROOT)
    boost_branch = _deduce_boost_branch() if args.boost_branch is None else args.boost_branch

    if args.build_kind == 'b2':
        _b2_build(
            source_dir=args.source_dir,
            toolset=args.toolset,
            cxxstd=args.cxxstd,
            variant=args.variant,
            stdlib=args.stdlib,
            address_model=args.address_model,
            separate_compilation=args.separate_compilation,
            use_ts_executor=args.use_ts_executor,
            address_sanitizer=args.address_sanitizer,
            undefined_sanitizer=args.undefined_sanitizer,
            clean=args.clean,
            boost_branch=boost_branch,
            db=args.db,
            server_host=args.server_host,
            coverage=args.coverage,
            valgrind=args.valgrind
        )
    elif args.build_kind == 'cmake':
        cmake_build(
            source_dir=args.source_dir,
            boost_branch=boost_branch,
            generator=args.generator,
            build_shared_libs=args.build_shared_libs,
            clean=args.clean,
            build_type=args.cmake_build_type,
            cxxstd=args.cxxstd,
            db=args.db,
            server_host=args.server_host
        )
    elif args.build_kind == 'cmake-noopenssl':
        cmake_noopenssl_build(
            source_dir=args.source_dir,
            boost_branch=args.boost_branch,
            generator=args.generator
        )
    elif args.build_kind == 'find-package-b2':
        find_package_b2_test(
            source_dir=args.source_dir,
            boost_branch=args.boost_branch,
            generator=args.generator
        )
    elif args.build_kind == 'fuzz':
        # Fuzzing uses some Python features only available in newer CIs
        from .fuzz import fuzz_build
        fuzz_build(
            source_dir=args.source_dir,
            boost_branch=boost_branch,
            clean=args.clean,
            db=args.db,
            server_host=args.server_host
        )
    else:
        _doc_build(
            source_dir=args.source_dir,
            boost_branch=boost_branch,
            clean=args.clean,
        )
