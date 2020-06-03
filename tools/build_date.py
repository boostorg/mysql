#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from sys import argv
from subprocess import check_call
from os import mkdir, chdir, makedirs

VERSION_TAG = 'v2.4.1'

def usage():
    print('Usage: {} <install-dir>'.format(argv[0]))
    exit(1)

def main():
    if len(argv) != 2:
        usage()
    install_dir = argv[1]
    print('Installing date in ', install_dir)
    makedirs(install_dir)
    check_call(['git', 'clone', 'https://github.com/HowardHinnant/date.git'])
    chdir('date')
    check_call(['git', 'checkout', VERSION_TAG])
    mkdir('build')
    chdir('build')
    check_call([
        'cmake',
        '-G',
        'Ninja',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DENABLE_DATE_TESTING=OFF',
        '-DUSE_SYSTEM_TZ_DB=ON',
        '-DCMAKE_INSTALL_PREFIX={}'.format(install_dir),
        '..'
    ])
    check_call(['cmake', '--build', '.', '--target', 'install'])
    
if __name__ == '__main__':
    main()
