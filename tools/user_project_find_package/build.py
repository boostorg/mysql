#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from sys import argv
from os import mkdir, chdir, path
from shutil import copy
from subprocess import check_call

PROJECT_DIR = 'user_project_find_package'

def main():
    cmake_args = argv[1:]
    this_dir = path.dirname(path.realpath(__file__))
    mkdir(PROJECT_DIR)
    copy(path.join(this_dir, 'CMakeLists.txt'), PROJECT_DIR)
    copy(path.join(this_dir, '..', '..', 'example', 'query_sync.cpp'), PROJECT_DIR)
    chdir(PROJECT_DIR)
    mkdir('build')
    chdir('build')
    check_call(['cmake'] + cmake_args + ['-G', 'Ninja', '..'])
    check_call(['cmake', '--build', '.'])
    check_call(['ctest', '--output-on-failure'])
    
if __name__ == '__main__':
    main()
    