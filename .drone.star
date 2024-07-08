#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

_triggers = { "branch": [ "master", "develop" ] }
_container_tag = 'c3f5316cc19bf3c0f7a83e31dec58139581f5764'
_win_container_tag = 'e7bd656c3515263f9b3c69a2d73d045f6a0fed72'


def _image(name):
    return 'ghcr.io/anarthal-containers/{}:{}'.format(name, _container_tag)

def _win_image(name):
    return 'ghcr.io/anarthal-containers/{}:{}'.format(name, _win_container_tag)


def _b2_command(
    source_dir,
    toolset,
    cxxstd,
    variant,
    server_host='127.0.0.1',
    stdlib='native',
    address_model='64',
    separate_compilation=1,
    use_ts_executor=0,
    address_sanitizer=0,
    undefined_sanitizer=0,
    valgrind=0,
    fail_if_no_openssl=1
):
    return 'python tools/ci/main.py ' + \
                '--source-dir="{}" '.format(source_dir) + \
                'b2 ' + \
                '--server-host={} '.format(server_host) + \
                '--toolset={} '.format(toolset) + \
                '--cxxstd={} '.format(cxxstd) + \
                '--variant={} '.format(variant) + \
                '--stdlib={} '.format(stdlib) + \
                '--address-model={} '.format(address_model) + \
                '--separate-compilation={} '.format(separate_compilation) + \
                '--use-ts-executor={} '.format(use_ts_executor) + \
                '--address-sanitizer={} '.format(address_sanitizer) + \
                '--undefined-sanitizer={} '.format(undefined_sanitizer) + \
                '--valgrind={} '.format(valgrind) + \
                '--fail-if-no-openssl={} '.format(fail_if_no_openssl)


def _cmake_command(
    source_dir,
    server_host='127.0.0.1',
    db='mysql8',
    generator='Ninja',
    cmake_build_type='Debug',
    build_shared_libs=0,
    cxxstd='20',
    install_test=1
):
    return 'python tools/ci/main.py ' + \
                '--source-dir="{}" '.format(source_dir) + \
                'cmake ' + \
                '--server-host={} '.format(server_host) + \
                '--db={} '.format(db) + \
                '--generator="{}" '.format(generator) + \
                '--cmake-build-type={} '.format(cmake_build_type) + \
                '--build-shared-libs={} '.format(build_shared_libs) + \
                '--cxxstd={} '.format(cxxstd) + \
                '--install-test={} '.format(install_test)


def _find_package_b2_command(source_dir, generator):
    return 'python tools/ci/main.py ' + \
                '--source-dir="{}" '.format(source_dir) + \
                'find-package-b2 ' + \
                '--generator="{}" '.format(generator)


def _pipeline(
    name,
    image,
    os,
    command,
    db,
    arch='amd64',
    disable_aslr=False
):
    steps = []
    if disable_aslr:
        steps.append({
            "name": "Disable ASLR",
            "image": image,
            "pull": "if-not-exists",
            "privileged": True,
            "commands": ["echo 0 | tee /proc/sys/kernel/randomize_va_space"]
        })
    steps.append({
        "name": "Build and run",
        "image": image,
        "pull": "if-not-exists",
        "privileged": arch == "arm64",
        "volumes":[{
            "name": "mysql-socket",
            "path": "/var/run/mysqld"
        }] if db != None else [],
        "commands": [command]
    })

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
        "steps": steps,
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
    use_ts_executor = 0,
    address_sanitizer=0,
    undefined_sanitizer=0,
    valgrind=0,
    arch='amd64',
    fail_if_no_openssl=1
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
        use_ts_executor=use_ts_executor,
        address_sanitizer=address_sanitizer,
        undefined_sanitizer=undefined_sanitizer,
        valgrind=valgrind,
        fail_if_no_openssl=fail_if_no_openssl
    )
    return _pipeline(
        name=name,
        image=image,
        os='linux',
        command=command,
        db='mysql8',
        arch=arch,
        disable_aslr=True
    )


def windows_b2(
    name,
    image,
    toolset,
    cxxstd,
    variant,
    address_model = '64',
    use_ts_executor = 0
):
    command = _b2_command(
        source_dir='$Env:DRONE_WORKSPACE',
        toolset=toolset,
        cxxstd=cxxstd,
        variant=variant,
        address_model=address_model,
        server_host='127.0.0.1',
        use_ts_executor=use_ts_executor
    )
    return _pipeline(name=name, image=image, os='windows', command=command, db=None)


def linux_cmake(
    name,
    image,
    build_shared_libs=0,
    cmake_build_type='Debug',
    cxxstd='20',
    db='mysql8',
    install_test=1
):
    command = _cmake_command(
        source_dir='$(pwd)',
        build_shared_libs=build_shared_libs,
        cmake_build_type=cmake_build_type,
        cxxstd=cxxstd,
        db=db,
        server_host='mysql',
        install_test=install_test
    )
    return _pipeline(name=name, image=image, os='linux', command=command, db=db)


def linux_cmake_noopenssl(name):
    command = 'python tools/ci/main.py ' + \
                '--source-dir=$(pwd) ' + \
                'cmake-noopenssl ' + \
                '--generator=Ninja '
    return _pipeline(name=name, image=_image('build-noopenssl'), os='linux', command=command, db=None)


def linux_cmake_nointeg(name):
    command = 'python tools/ci/main.py ' + \
                '--source-dir=$(pwd) ' + \
                'cmake-nointeg ' + \
                '--generator=Ninja '
    return _pipeline(name=name, image=_image('build-gcc13'), os='linux', command=command, db=None)


def windows_cmake(
    name,
    build_shared_libs=0
):
    command = _cmake_command(
        source_dir='$Env:DRONE_WORKSPACE',
        build_shared_libs=build_shared_libs,
        generator='Visual Studio 17 2022',
        db='mysql8',
        server_host='127.0.0.1'
    )
    return _pipeline(
        name=name,
        image=_win_image('build-msvc14_3'),
        os='windows',
        command=command,
        db=None
    )


def find_package_b2_linux(name):
    command = _find_package_b2_command(source_dir='$(pwd)', generator='Ninja')
    return _pipeline(name=name, image=_image('build-gcc13'), os='linux', command=command, db=None)


def find_package_b2_windows(name):
    command = _find_package_b2_command(source_dir='$Env:DRONE_WORKSPACE', generator='Visual Studio 17 2022')
    return _pipeline(name=name, image=_win_image('build-msvc14_3'), os='windows', command=command, db=None)


def docs(name):
    return _pipeline(
        name=name,
        image=_image('build-docs'),
        os='linux',
        command='python tools/ci/main.py --source-dir=$(pwd) docs',
        db=None
    )


def main(ctx):
    return [
        # CMake Linux
        linux_cmake('Linux CMake MySQL 5.x',      _image('build-gcc14'), db='mysql5', build_shared_libs=0),
        linux_cmake('Linux CMake MariaDB',        _image('build-gcc14'), db='mariadb', build_shared_libs=1),
        linux_cmake('Linux CMake cmake 3.8',      _image('build-cmake3_8'), cxxstd='11', install_test=0),
        linux_cmake('Linux CMake gcc Release',    _image('build-gcc14'), cmake_build_type='Release'),
        linux_cmake('Linux CMake gcc MinSizeRel', _image('build-gcc14'), cmake_build_type='MinSizeRel'),
        linux_cmake_noopenssl('Linux CMake no OpenSSL'),
        linux_cmake_nointeg('Linux CMake without integration tests'),

        # CMake Windows
        windows_cmake('Windows CMake static', build_shared_libs=0),
        windows_cmake('Windows CMake shared', build_shared_libs=1),

        # find_package with B2 distribution
        find_package_b2_linux('Linux find_package b2 distribution'),
        find_package_b2_windows('Windows find_package b2 distribution'),

        # B2 Linux
        linux_b2('Linux B2 clang-3.6',            _image('build-clang3_6'),      toolset='clang-3.6', cxxstd='11,14'),
        linux_b2('Linux B2 clang-7',              _image('build-clang7'),        toolset='clang-7',   cxxstd='14,17'),
        linux_b2('Linux B2 clang-11',             _image('build-clang11'),       toolset='clang-11',  cxxstd='20'),
        linux_b2('Linux B2 clang-14-header-only', _image('build-clang14'),       toolset='clang-14',  cxxstd='11,20', separate_compilation=0),
        linux_b2('Linux B2 clang-14-libc++',      _image('build-clang14'),       toolset='clang-14',  cxxstd='20', stdlib='libc++'),
        linux_b2('Linux B2 clang-14-arm64',       _image('build-clang14'),       toolset='clang-14',  cxxstd='20', arch='arm64'),
        linux_b2('Linux B2 clang-16-sanit',       _image('build-clang16'),       toolset='clang-16',  cxxstd='20', address_sanitizer=1, undefined_sanitizer=1),
        linux_b2('Linux B2 clang-16-i386-sanit',  _image('build-clang16-i386'),  toolset='clang-16',  cxxstd='20', address_model=32, address_sanitizer=1, undefined_sanitizer=1),
        linux_b2('Linux B2 clang-17',             _image('build-clang17'),       toolset='clang-17',  cxxstd='20'),
        linux_b2('Linux B2 clang-18',             _image('build-clang18'),       toolset='clang-18',  cxxstd='23'),
        linux_b2('Linux B2 gcc-5',                _image('build-gcc5'),          toolset='gcc-5',     cxxstd='11'), # gcc-5 C++14 doesn't like my constexpr field_view
        linux_b2('Linux B2 gcc-5-ts-executor',    _image('build-gcc5'),          toolset='gcc-5',     cxxstd='11', use_ts_executor=1),
        linux_b2('Linux B2 gcc-6',                _image('build-gcc6'),          toolset='gcc-6',     cxxstd='14,17'),
        linux_b2('Linux B2 gcc-10',               _image('build-gcc10'),         toolset='gcc-10',    cxxstd='17,20'),
        linux_b2('Linux B2 gcc-11',               _image('build-gcc11'),         toolset='gcc-11',    cxxstd='20'),
        linux_b2('Linux B2 gcc-11-arm64',         _image('build-gcc11'),         toolset='gcc-11',    cxxstd='11,20', arch='arm64', variant='release'),
        linux_b2('Linux B2 gcc-11-arm64-sanit',   _image('build-gcc11'),         toolset='gcc-11',    cxxstd='20',    arch='arm64', variant='debug'),
        linux_b2('Linux B2 gcc-13',               _image('build-gcc13'),         toolset='gcc-13',    cxxstd='20', variant='release'),
        linux_b2('Linux B2 gcc-14',               _image('build-gcc14'),         toolset='gcc-14',    cxxstd='23', variant='release'),
        linux_b2('Linux B2 gcc-14-sanit',         _image('build-gcc14'),         toolset='gcc-14',    cxxstd='23', variant='debug', address_sanitizer=1, undefined_sanitizer=1),
        linux_b2('Linux B2 gcc-14-valgrind',      _image('build-gcc14'),         toolset='gcc-14',    cxxstd='23', variant='debug', valgrind=1),
        linux_b2('Linux B2 noopenssl',            _image('build-noopenssl'),     toolset='gcc',       cxxstd='11', fail_if_no_openssl=0),

        # B2 Windows
        windows_b2('Windows B2 msvc14.1 32-bit',      _win_image('build-msvc14_1'), toolset='msvc-14.1', cxxstd='11,14,17', variant='release',       address_model='32'),
        windows_b2('Windows B2 msvc14.1 64-bit',      _win_image('build-msvc14_1'), toolset='msvc-14.1', cxxstd='14,17',    variant='release'),
        windows_b2('Windows B2 msvc14.2',             _win_image('build-msvc14_2'), toolset='msvc-14.2', cxxstd='14,17',    variant='release'),
        windows_b2('Windows B2 msvc14.3',             _win_image('build-msvc14_3'), toolset='msvc-14.3', cxxstd='17,20',    variant='debug,release'),
        windows_b2('Windows B2 msvc14.3-ts-executor', _win_image('build-msvc14_3'), toolset='msvc-14.3', cxxstd='20',       variant='release',       use_ts_executor=1),

        # Docs
        docs('Linux docs')
    ]

