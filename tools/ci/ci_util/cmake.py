#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from pathlib import Path
import os
from typing import Optional
from .common import run, BOOST_ROOT, IS_WINDOWS, mkdir_and_cd
from .db_setup import db_setup
from .install_boost import install_boost


def _cmake_prefix_path(extra_path: Optional[Path] = None) -> str:
    prefix_path_args = [extra_path, 'C:\\openssl-64' if IS_WINDOWS else None]
    return ';'.join(str(p) for p in prefix_path_args if p is not None)


def _cmake_bool(value: bool) -> str:
    return 'ON' if value else 'OFF'


# Regular CMake build
def cmake_build(
    source_dir: Path,
    boost_branch: str,
    generator: str,
    build_shared_libs: bool,
    clean: bool,
    build_type: str,
    cxxstd: str,
    db: str,
    server_host: str,
) -> None:
    # Config
    cmake_distro = Path(os.path.expanduser('~')).joinpath('cmake-distro')
    test_folder = BOOST_ROOT.joinpath('libs', 'mysql', 'test', 'cmake_test')
    os.environ['CMAKE_BUILD_PARALLEL_LEVEL'] = '4'

    # Get Boost
    install_boost(
        source_dir=source_dir,
        boost_branch=boost_branch,
        clean=clean,
    )

    # Setup DB
    db_setup(source_dir, db, server_host)

    # Build the library, run the tests, and install
    mkdir_and_cd(BOOST_ROOT.joinpath('__build_cmake_test__'))
    run([
        'cmake',
        '-G',
        generator,
        '-DCMAKE_PREFIX_PATH={}'.format(_cmake_prefix_path()),
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
    ] + (['-DCMAKE_CXX_STANDARD={}'.format(cxxstd)] if cxxstd else []) + [
        '-DBOOST_INCLUDE_LIBRARIES=mysql',
        '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        '-DCMAKE_INSTALL_PREFIX={}'.format(cmake_distro),
        '-DBOOST_MYSQL_INTEGRATION_TESTS=ON',
        '-DBUILD_TESTING=ON',
        '-DBoost_VERBOSE=ON',
        '-DCMAKE_INSTALL_MESSAGE=NEVER',
        '..'
    ])
    run(['cmake', '--build', '.', '--target', 'tests', '--config', build_type])
    run(['ctest', '--output-on-failure', '--build-config', build_type])
    run(['cmake', '--build', '.', '--target', 'install', '--config', build_type])

    # The library can be consumed using add_subdirectory
    mkdir_and_cd(test_folder.joinpath('__build_cmake_subdir_test__'))
    run([
        'cmake',
        '-G',
        generator,
        '-DCMAKE_PREFIX_PATH={}'.format(_cmake_prefix_path()),
        '-DBOOST_CI_INSTALL_TEST=OFF',
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        '..'
    ])
    run(['cmake', '--build', '.', '--config', build_type])
    run(['ctest', '--output-on-failure', '--build-config', build_type])

    # The library can be consumed using find_package on a Boost distro built by cmake
    mkdir_and_cd(test_folder.joinpath('__build_cmake_install_test__'))
    run([
        'cmake',
        '-G',
        generator,
        '-DBOOST_CI_INSTALL_TEST=ON',
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DBUILD_SHARED_LIBS={}'.format(_cmake_bool(build_shared_libs)),
        '-DCMAKE_PREFIX_PATH={}'.format(_cmake_prefix_path(cmake_distro)),
        '..'
    ])
    run(['cmake', '--build', '.', '--config', build_type])
    run(['ctest', '--output-on-failure', '--build-config', build_type])


# Check that we bail out correctly when no OpenSSL is available
def cmake_noopenssl_build(
    source_dir: Path,
    boost_branch: str,
    generator: str
):
    # Config
    os.environ['CMAKE_BUILD_PARALLEL_LEVEL'] = '4'
    build_type = 'Release'

    # Get Boost
    install_boost(
        source_dir=source_dir,
        boost_branch=boost_branch,
        clean=True,
    )

    # Build Boost and install. This won't build the library if there is no OpenSSL,
    # but shouldn't cause any error.
    mkdir_and_cd(BOOST_ROOT.joinpath('__build_cmake_test__'))
    run([
        'cmake',
        '-G',
        generator,
        '-DBOOST_INCLUDE_LIBRARIES=mysql',
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DBOOST_MYSQL_INTEGRATION_TESTS=ON',
        '-DBUILD_TESTING=ON',
        '-DBoost_VERBOSE=ON',
        '-DCMAKE_INSTALL_MESSAGE=NEVER',
        '..'
    ])
    run(['cmake', '--build', '.', '--target', 'tests', '--config', build_type])
    run(['ctest', '--output-on-failure', '--build-config', build_type])
    run(['cmake', '--build', '.', '--target', 'install', '--config', build_type])



# Check that CMake can consume a Boost distribution created by b2
def find_package_b2_test(
    source_dir: Path,
    boost_branch: str,
    generator: str
) -> None:
    # Config
    b2_distro = Path(os.path.expanduser('~')).joinpath('b2-distro')
    build_type = 'Release'
    prefix_path = _cmake_prefix_path(b2_distro)

    # Get Boost
    install_boost(
        source_dir=source_dir,
        boost_branch=boost_branch,
        clean=True,
    )

    # Generate a b2 Boost distribution
    run([
        'b2',
        '--prefix={}'.format(b2_distro),
        '--with-context',
        '--with-test',
        '--with-json',
        '--with-url',
        '--with-charconv',
        '-d0',
        'install'
    ])

    # Check that the library can be consumed using find_package on the distro above
    mkdir_and_cd(BOOST_ROOT.joinpath('libs', 'mysql', 'test', 'cmake_b2_test', '__build_cmake_b2_test__'))
    run([
        'cmake',
        '-G',
        generator,
        '-DCMAKE_PREFIX_PATH={}'.format(prefix_path),
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DBUILD_TESTING=ON',
        '..'
    ])
    run(['cmake', '--build', '.', '--config', build_type])
    run(['ctest', '--output-on-failure', '--build-config', build_type])

    # Same as the above, but for separate-build mode
    mkdir_and_cd(BOOST_ROOT.joinpath('libs', 'mysql', 'test', 'cmake_b2_separate_compilation_test', '__build__'))
    run([
        'cmake',
        '-G',
        generator,
        '-DCMAKE_PREFIX_PATH={}'.format(prefix_path),
        '-DCMAKE_BUILD_TYPE={}'.format(build_type),
        '-DBUILD_TESTING=ON',
        '..'
    ])
    run(['cmake', '--build', '.', '--config', build_type])
    run(['ctest', '--output-on-failure', '--build-config', build_type])

