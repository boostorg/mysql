#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from pathlib import Path
import os
import enum
from shutil import rmtree, ignore_patterns, copytree
import stat
from typing import List
import subprocess
import sys


REPO_BASE = Path(os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..', '..')).absolute()
BOOST_ROOT = Path(os.path.expanduser('~')).joinpath('boost-root')
IS_WINDOWS = os.name == 'nt'


def run(args: List[str]) -> None:
    print('+ ', args, flush=True)
    subprocess.run(args, check=True)


def mkdir_and_cd(path: Path) -> None:
    os.makedirs(str(path), exist_ok=True)
    os.chdir(str(path))


class BoostInstallType(enum.Enum):
    mysql = 0
    docs = 1


def install_boost(
    boost_root: Path,
    source_dir: Path,
    clean: bool = False,
    install_type: BoostInstallType = BoostInstallType.mysql,
    branch: str = 'develop'
) -> None:
    assert boost_root.is_absolute()
    assert source_dir.is_absolute()
    lib_dir = boost_root.joinpath('libs', 'mysql')
    
    def remove_readonly(func, path, _):
        os.chmod(path, stat.S_IWRITE)
        func(path)

    # See if target exists
    if boost_root.exists() and clean:
        rmtree(str(boost_root), onerror=remove_readonly)
    
    # Clone Boost
    is_clean = not boost_root.exists()
    if is_clean:
        run(['git', 'clone', '-b', branch, '--depth', '1', 'https://github.com/boostorg/boost.git', str(boost_root)])
    os.chdir(str(boost_root))

    # Put our library inside boost root
    supports_dir_exist_ok = sys.version_info.minor >= 8
    if lib_dir.exists() and (clean or not supports_dir_exist_ok):
        rmtree(str(lib_dir), onerror=remove_readonly)
    copytree(
        str(source_dir),
        str(lib_dir),
        ignore=ignore_patterns('__build*__', '.git'),
        **({ 'dirs_exist_ok': True } if supports_dir_exist_ok else {}) # type: ignore
    )

    # Install Boost dependencies
    if is_clean:
        run(["git", "config", "submodule.fetchJobs", "8"])
        if install_type == BoostInstallType.mysql:
            submodules = ['tools/boostdep']
        else:
            submodules = [
                'libs/context',
                'tools/boostdep',
                'tools/boostbook',
                'tools/docca',
                'tools/quickbook'
            ]
        for subm in submodules:
            run(["git", "submodule", "update", "-q", "--init", subm])
        if install_type == BoostInstallType.mysql:
            run(["python", "tools/boostdep/depinst/depinst.py", "--include", "example", "mysql"])
        else:
            run(['python', 'tools/boostdep/depinst/depinst.py', '../tools/quickbook'])
    
    # Bootstrap
    if is_clean:
        if IS_WINDOWS:
            run(['cmd', '/q', '/c', 'bootstrap.bat'])
        else:
            run(['bash', 'bootstrap.sh'])
        run(['b2', 'headers'])
