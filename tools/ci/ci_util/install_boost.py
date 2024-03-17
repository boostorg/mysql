#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from shutil import rmtree, ignore_patterns, copytree
import stat
import sys
from pathlib import Path
import os
from .common import run, IS_WINDOWS, BOOST_ROOT


def install_boost(
    source_dir: Path,
    boost_branch: str,
    clean: bool,
    docs_install: bool = False
) -> None:
    assert source_dir.is_absolute()
    lib_dir = BOOST_ROOT.joinpath('libs', 'mysql')
    
    def remove_readonly(func, path, _):
        os.chmod(path, stat.S_IWRITE)
        func(path)

    # See if target exists
    if BOOST_ROOT.exists() and clean:
        rmtree(str(BOOST_ROOT), onerror=remove_readonly)
    
    # Clone Boost
    is_clean = not BOOST_ROOT.exists()
    if is_clean:
        run(['git', 'clone', '-b', boost_branch, '--depth', '1', 'https://github.com/boostorg/boost.git', str(BOOST_ROOT)])
    os.chdir(str(BOOST_ROOT))

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
        if docs_install:
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
        if docs_install:
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
