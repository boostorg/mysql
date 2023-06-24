#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

_triggers = { "branch": [ "master", "develop", "drone*", "feature/*", "bugfix/*", "fix/*", "pr/*" ] }
_container_tag = 'c94b77a716a0cc2cf5f489d9a97e9a0aefa7c0de'
_win_container_tag = '6efce57d289bfa028da8d4a9b50158f1f073984f'


def _image(name):
    return 'ghcr.io/anarthal-containers/{}:{}'.format(name, _container_tag)

def _win_image(name):
    return 'ghcr.io/anarthal-containers/{}:{}'.format(name, _win_container_tag)


def _b2_command(
    source_dir,
    toolset,
    cxxstd,
    variant,
    stdlib='native',
    address_model='64',
    server_host='localhost',
    separate_compilation=1,
    address_sanitizer=0,
    undefined_sanitizer=0
):
    return 'python tools/ci.py ' + \
                '--clean=1 ' + \
                '--build-kind=b2 ' + \
                '--source-dir="{}" '.format(source_dir) + \
                '--toolset={} '.format(toolset) + \
                '--cxxstd={} '.format(cxxstd) + \
                '--variant={} '.format(variant) + \
                '--stdlib={} '.format(stdlib) + \
                '--address-model={} '.format(address_model) + \
                '--server-host={} '.format(server_host) + \
                '--separate-compilation={} '.format(separate_compilation) + \
                '--address-sanitizer={} '.format(address_sanitizer) + \
                '--undefined-sanitizer={} '.format(undefined_sanitizer)


def _cmake_command(
    source_dir,
    build_shared_libs=0,
    cmake_build_type='Debug',
    cxxstd='20',
    valgrind=0,
    coverage=0,
    standalone_tests=1,
    add_subdir_tests=1,
    install_tests=1,
    generator='Ninja',
    db='mysql8',
    server_host='localhost'
):
    return 'python tools/ci.py ' + \
                '--build-kind=cmake ' + \
                '--clean=1 ' + \
                '--generator="{}" '.format(generator) + \
                '--source-dir="{}" '.format(source_dir) + \
                '--build-shared-libs={} '.format(build_shared_libs) + \
                '--cmake-build-type={} '.format(cmake_build_type) + \
                '--cxxstd={} '.format(cxxstd) + \
                '--valgrind={} '.format(valgrind) + \
                '--coverage={} '.format(coverage) + \
                '--cmake-standalone-tests={} '.format(standalone_tests) + \
                '--cmake-add-subdir-tests={} '.format(add_subdir_tests) + \
                '--cmake-install-tests={} '.format(install_tests) + \
                '--db={} '.format(db) + \
                '--server-host={} '.format(server_host)


def _pipeline(
    name,
    image,
    os,
    command,
    db,
    arch='amd64'
):
    return {
        "name": name,
        "kind": "pipeline",
        "type": "docker",
        "trigger": _triggers,
        "platform": {
            "os": os,
            "arch": arch
        },
        "clone": {
            "retries": 5
        },
        "node": {},
        "steps": [{
            "name": "Everything",
            "image": image,
            "pull": "if-not-exists",
            "volumes":[{
                "name": "mysql-socket",
                "path": "/var/run/mysqld"
            }] if db != None else [],
            "commands": [command],
            "environment": {
                "CODECOV_TOKEN": {
                    "from_secret": "CODECOV_TOKEN"
                }
            }
        }],
        "services": [{
            "name": "mysql",
            "image": _image(db),
            "volumes": [{
                "name": "mysql-socket",
                "path": "/var/run/mysqld"
            }]
        }] if db != None else [],
        "volumes": [{
            "name": "mysql-socket",
            "temp": {}
        }] if db != None else []
    }


def linux_b2(
    name,
    image,
    toolset,
    cxxstd,
    variant='debug,release',
    stdlib='native',
    address_model='64',
    separate_compilation=1,
    address_sanitizer=0,
    undefined_sanitizer=0,
    arch='amd64',
):
    command = _b2_command(
        source_dir='$(pwd)',
        toolset=toolset,
        cxxstd=cxxstd,
        variant=variant,
        stdlib=stdlib,
        address_model=address_model,
        server_host='mysql',
        separate_compilation=separate_compilation,
        address_sanitizer=address_sanitizer,
        undefined_sanitizer=undefined_sanitizer
    )
    return _pipeline(
        name=name,
        image=image,
        os='linux',
        command=command,
        db='mysql8',
        arch=arch
    )


def windows_b2(
    name,
    image,
    toolset,
    cxxstd,
    variant,
    address_model = '64'
):
    command = _b2_command(
        source_dir='$Env:DRONE_WORKSPACE',
        toolset=toolset,
        cxxstd=cxxstd,
        variant=variant,
        address_model=address_model,
        server_host='localhost'
    )
    return _pipeline(name=name, image=image, os='windows', command=command, db=None)


def linux_cmake(
    name,
    image,
    build_shared_libs=0,
    cmake_build_type='Debug',
    cxxstd='20',
    valgrind=0,
    coverage=0,
    standalone_tests=1,
    add_subdir_tests=1,
    install_tests=1,
    db='mysql8'
):
    command = _cmake_command(
        source_dir='$(pwd)',
        build_shared_libs=build_shared_libs,
        cmake_build_type=cmake_build_type,
        cxxstd=cxxstd,
        valgrind=valgrind,
        coverage=coverage,
        standalone_tests=standalone_tests,
        add_subdir_tests=add_subdir_tests,
        install_tests=install_tests,
        db=db,
        server_host='mysql'
    )
    return _pipeline(name=name, image=image, os='linux', command=command, db=db)


def windows_cmake(
    name,
    build_shared_libs=0
):
    command = _cmake_command(
        source_dir='$Env:DRONE_WORKSPACE',
        build_shared_libs=build_shared_libs,
        generator='Visual Studio 17 2022',
        db='mysql8',
        server_host='localhost'
    )
    return _pipeline(
        name=name,
        image=_image('build-msvc14_3'),
        os='windows',
        command=command,
        db=None
    )


def docs():
    return _pipeline(
        name='Linux docs',
        image=_image('build-docs'),
        os='linux',
        command='python tools/ci.py --build-kind=docs --clean=1 --source-dir=$(pwd)',
        db=None
    )


def main(ctx):
    return [
        # CMake Linux
        linux_cmake('Linux CMake valgrind',       _image('build-gcc11'), valgrind=1, build_shared_libs=0),
        linux_cmake('Linux CMake coverage',       _image('build-gcc11'), coverage=1, build_shared_libs=0),
        linux_cmake('Linux CMake MySQL 5.x',      _image('build-clang14'), db='mysql5', build_shared_libs=0),
        linux_cmake('Linux CMake MariaDB',        _image('build-clang14'), db='mariadb', build_shared_libs=1),
        linux_cmake('Linux CMake cmake 3.8',      _image('build-cmake3_8'), cxxstd='11', standalone_tests=0, install_tests=0),
        linux_cmake('Linux CMake no OpenSSL',     _image('build-noopenssl'), standalone_tests=0, add_subdir_tests=0, install_tests=0),
        linux_cmake('Linux CMake gcc Release',    _image('build-gcc11'), cmake_build_type='Release'),
        linux_cmake('Linux CMake gcc MinSizeRel', _image('build-gcc11'), cmake_build_type='MinSizeRel'),

        # CMake Windows
        windows_cmake('Windows CMake static', build_shared_libs=0),
        windows_cmake('Windows CMake shared', build_shared_libs=1),

        # B2 Linux
        linux_b2('Linux B2 clang-3.6',            _image('build-clang3_6'),      toolset='clang-3.6', cxxstd='11,14'),
        linux_b2('Linux B2 clang-7',              _image('build-clang7'),        toolset='clang-7',   cxxstd='14,17'),
        linux_b2('Linux B2 clang-11',             _image('build-clang11'),       toolset='clang-11',  cxxstd='20'),
        linux_b2('Linux B2 clang-14-header-only', _image('build-clang14'),       toolset='clang-14',  cxxstd='11,20', separate_compilation=0),
        linux_b2('Linux B2 clang-14-libc++',      _image('build-clang14'),       toolset='clang-14',  cxxstd='20', stdlib='libc++'),
        linux_b2('Linux B2 clang-14-arm64',       _image('build-clang14'),       toolset='clang-14',  cxxstd='20', arch='arm64'),
        linux_b2('Linux B2 clang-16-sanit',       _image('build-clang16'),       toolset='clang-16',  cxxstd='20', address_sanitizer=1, undefined_sanitizer=1),
        linux_b2('Linux B2 clang-16-i386-sanit',  _image('build-clang16-i386'),  toolset='clang-16',  cxxstd='20', address_model=32, address_sanitizer=1, undefined_sanitizer=1),
        linux_b2('Linux B2 gcc-5',                _image('build-gcc5'),          toolset='gcc-5',     cxxstd='11'), # gcc-5 C++14 doesn't like my constexpr field_view
        linux_b2('Linux B2 gcc-6',                _image('build-gcc6'),          toolset='gcc-6',     cxxstd='14,17'),
        linux_b2('Linux B2 gcc-10',               _image('build-gcc10'),         toolset='gcc-10',    cxxstd='17,20'),
        linux_b2('Linux B2 gcc-11',               _image('build-gcc11'),         toolset='gcc-11',    cxxstd='20'),
        linux_b2('Linux B2 gcc-11-arm64',         _image('build-gcc11'),         toolset='gcc-11',    cxxstd='11,20', arch='arm64', variant='release'),
        linux_b2('Linux B2 gcc-11-arm64-sanit',   _image('build-gcc11'),         toolset='gcc-11',    cxxstd='20',    arch='arm64', variant='debug'),
        linux_b2('Linux B2 gcc-13',               _image('build-gcc13'),         toolset='gcc-13',    cxxstd='20', variant='release'),
        linux_b2('Linux B2 gcc-13-sanit',         _image('build-gcc13'),         toolset='gcc-13',    cxxstd='20', variant='debug', address_sanitizer=1, undefined_sanitizer=1),

        # B2 Windows
        windows_b2('Windows B2 msvc14.1 32-bit', _win_image('build-msvc14_1'), toolset='msvc-14.1', cxxstd='11,14,17', variant='release',       address_model='32'),
        windows_b2('Windows B2 msvc14.1 64-bit', _win_image('build-msvc14_1'), toolset='msvc-14.1', cxxstd='14,17',    variant='release',       address_model='64'),
        windows_b2('Windows B2 msvc14.2',        _win_image('build-msvc14_2'), toolset='msvc-14.2', cxxstd='14,17',    variant='release',       address_model='64'),
        windows_b2('Windows B2 msvc14.3',        _win_image('build-msvc14_3'), toolset='msvc-14.3', cxxstd='17,20',    variant='debug,release', address_model='64'),

        # Docs
        docs()
    ]

