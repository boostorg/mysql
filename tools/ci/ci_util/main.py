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
from .common import IS_WINDOWS, BOOST_ROOT, install_boost, BoostInstallType, run
from .db_setup import db_setup


def _add_to_path(path: Path) -> None:
    sep = ';' if IS_WINDOWS  else ':'
    os.environ['PATH'] = '{}{}{}'.format(path, sep, os.environ["PATH"])


def _mkdir_and_cd(path: Path) -> None:
    os.makedirs(str(path), exist_ok=True)
    os.chdir(str(path))


def _cmake_bool(value: bool) -> str:
    return 'ON' if value else 'OFF'


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
      + [
        'warnings=extra',
        'warnings-as-errors=on',
        '-j4',
        'libs/mysql/test',
        'libs/mysql/test/integration//boost_mysql_integrationtests',
        'libs/mysql/test/thread_safety',
        'libs/mysql/example'
    ])


def _build_prefix_path(*paths: Union[str, Path]) -> str:
    return ';'.join(str(p) for p in paths)


def _cmake_build(
    source_dir: Path,
    generator: str,
    build_shared_libs: bool,
    valgrind: bool,
    clean: bool,
    standalone_tests: bool,
    add_subdir_tests: bool,
    install_tests: bool,
    build_type: str,
    cxxstd: str,
    boost_branch: str,
    db: str,
    server_host: str,
) -> None:
    # Config
    home = Path(os.path.expanduser('~'))
    b2_distro = home.joinpath('b2-distro')
    cmake_distro = home.joinpath('cmake-distro')
    test_folder = BOOST_ROOT.joinpath('libs', 'mysql', 'test', 'cmake_test')
    os.environ['CMAKE_BUILD_PARALLEL_LEVEL'] = '4'
    if IS_WINDOWS:
        cmake_prefix_path = ['C:\\openssl-64']
    else:
        cmake_prefix_path = []
        old_ld_libpath = os.environ.get("LD_LIBRARY_PATH", "")
        os.environ['LD_LIBRARY_PATH'] = '{}/lib:{}'.format(b2_distro, old_ld_libpath)

    # Get Boost
    install_boost(
        BOOST_ROOT,
        source_dir,
        clean=clean,
        branch=boost_branch
    )

    # Setup DB
    db_setup(source_dir, db, server_host)

    # Generate "pre-built" b2 distro
    if standalone_tests:
        run([
            'b2',
            '--prefix={}'.format(b2_distro),
            '--with-context',
            '--with-test',
            '--with-json',
            '--with-url',
            '--with-charconv',
            '-d0',
        ] + (['cxxstd={}'.format(cxxstd)] if cxxstd else []) + [
            'install'
        ])


    # Build the library, run the tests, and install, from the superproject
    _mkdir_and_cd(BOOST_ROOT.joinpath('__build_cmake_test__'))
    run([
        'cmake',
        '-G',
        generator,
        '-DCMAKE_PREFIX_PATH={}'.format(_build_prefix_path(*cmake_prefix_path)),
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
    ] + (['-DCMAKE_CXX_STANDARD={}'.format(cxxstd)] if cxxstd else []) + [
        '-DBOOST_INCLUDE_LIBRARIES=mysql',
        '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        '-DCMAKE_INSTALL_PREFIX={}'.format(cmake_distro),
        '-DBUILD_TESTING=ON',
        '-DBoost_VERBOSE=ON',
        '-DCMAKE_INSTALL_MESSAGE=NEVER',
        '..'
    ])
    run(['cmake', '--build', '.', '--target', 'tests', '--config', build_type])
    run(['ctest', '--output-on-failure', '--build-config', build_type])
    if install_tests:
        run(['cmake', '--build', '.', '--target', 'install', '--config', build_type])

    # Library tests, using the b2 Boost distribution generated before (this tests our normal dev workflow)
    if standalone_tests:
        _mkdir_and_cd(BOOST_ROOT.joinpath('libs', 'mysql', '__build_standalone__'))
        run([
            'cmake',
            '-DCMAKE_PREFIX_PATH={}'.format(_build_prefix_path(b2_distro, *cmake_prefix_path)),
            '-DCMAKE_BUILD_TYPE={}'.format(build_type),
            '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        ] + (['-DCMAKE_CXX_STANDARD={}'.format(cxxstd)] if cxxstd else []) + [
            '-DBOOST_MYSQL_INTEGRATION_TESTS=ON',
            '-DBOOST_MYSQL_VALGRIND_TESTS={}'.format(_cmake_bool(valgrind)),
            '-G',
            generator,
            '..'
        ])
        run(['cmake', '--build', '.'])
        run(['ctest', '--output-on-failure', '--build-config', build_type])
    

    # Subdir tests, using add_subdirectory() (lib can be consumed using add_subdirectory)
    if add_subdir_tests:
        _mkdir_and_cd(test_folder.joinpath('__build_cmake_subdir_test__'))
        run([
            'cmake',
            '-G',
            generator,
            '-DCMAKE_PREFIX_PATH={}'.format(_build_prefix_path(*cmake_prefix_path)),
            '-DBOOST_CI_INSTALL_TEST=OFF',
            '-DCMAKE_BUILD_TYPE={}'.format(build_type),
            '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
            '..'
        ])
        run(['cmake', '--build', '.', '--config', build_type])
        run(['ctest', '--output-on-failure', '--build-config', build_type])

    # Subdir tests, using find_package with the library installed in the previous step
    # (library can be consumed using find_package on a distro built by cmake)
    if install_tests:
        _mkdir_and_cd(test_folder.joinpath('__build_cmake_install_test__'))
        run([
            'cmake',
            '-G',
            generator,
            '-DBOOST_CI_INSTALL_TEST=ON',
            '-DCMAKE_BUILD_TYPE={}'.format(build_type),
            '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
            '-DCMAKE_PREFIX_PATH={}'.format(_build_prefix_path(cmake_distro, *cmake_prefix_path)),
            '..'
        ])
        run(['cmake', '--build', '.', '--config', build_type])
        run(['ctest', '--output-on-failure', '--build-config', build_type])

    # Subdir tests, using find_package with the b2 distribution
    # (library can be consumed using find_package on a distro built by b2)
    if standalone_tests:
        _mkdir_and_cd(BOOST_ROOT.joinpath('libs', 'mysql', 'test', 'cmake_b2_test', '__build_cmake_b2_test__'))
        run([
            'cmake',
            '-G',
            generator,
            '-DCMAKE_PREFIX_PATH={}'.format(_build_prefix_path(*cmake_prefix_path, b2_distro)),
            '-DCMAKE_BUILD_TYPE={}'.format(build_type),
            '-DBUILD_TESTING=ON',
            '..'
        ])
        run(['cmake', '--build', '.', '--config', build_type])
        run(['ctest', '--output-on-failure', '--build-config', build_type])

        # Same as the above, but for separate-build mode
        _mkdir_and_cd(BOOST_ROOT.joinpath('libs', 'mysql', 'test', 'cmake_b2_separate_compilation_test', '__build__'))
        run([
            'cmake',
            '-G',
            generator,
            '-DCMAKE_PREFIX_PATH={}'.format(_build_prefix_path(*cmake_prefix_path, b2_distro)),
            '-DCMAKE_BUILD_TYPE={}'.format(build_type),
            '-DBUILD_TESTING=ON',
            '..'
        ])
        run(['cmake', '--build', '.', '--config', build_type])
        run(['ctest', '--output-on-failure', '--build-config', build_type])


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
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-kind', choices=['b2', 'cmake', 'fuzz', 'docs'], required=True)
    parser.add_argument('--source-dir', type=Path, required=True)
    parser.add_argument('--boost-branch', default=None) # None means "let this script deduce it"
    parser.add_argument('--generator', default='Ninja')
    parser.add_argument('--build-shared-libs', type=_str2bool, default=True)
    parser.add_argument('--valgrind', type=_str2bool, default=False)
    parser.add_argument('--coverage', type=_str2bool, default=False)
    parser.add_argument('--clean', type=_str2bool, default=False)
    parser.add_argument('--db', choices=['mysql5', 'mysql8', 'mariadb'], default='mysql8')
    parser.add_argument('--cmake-standalone-tests', type=_str2bool, default=True)
    parser.add_argument('--cmake-add-subdir-tests', type=_str2bool, default=True)
    parser.add_argument('--cmake-install-tests', type=_str2bool, default=True)
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
            coverage=args.coverage
        )
    elif args.build_kind == 'cmake':
        _cmake_build(
            source_dir=args.source_dir,
            generator=args.generator,
            build_shared_libs=args.build_shared_libs,
            valgrind=args.valgrind,
            clean=args.clean,
            standalone_tests=args.cmake_standalone_tests,
            add_subdir_tests=args.cmake_add_subdir_tests,
            install_tests=args.cmake_install_tests,
            build_type=args.cmake_build_type,
            cxxstd=args.cxxstd,
            boost_branch=boost_branch,
            db=args.db,
            server_host=args.server_host
        )
    elif args.build_kind == 'fuzz':
        # Fuzzing uses some Python features only available in newer CIs
        from .fuzz import fuzz_build
        fuzz_build(
            source_dir=args.source_dir,
            clean=args.clean,
            boost_branch=boost_branch,
            db=args.db,
            server_host=args.server_host
        )
    else:
        _doc_build(
            source_dir=args.source_dir,
            clean=args.clean,
            boost_branch=boost_branch
        )
