#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from subprocess import run
import os
import argparse

def get_executable_name(name):
    if os.name == 'nt':
        name += '.exe'
    return name

def run_sql_file(fname):
    print('Running SQL file: {}'.format(fname))
    with open(fname, 'rb') as f:
        sql = f.read()
    run([get_executable_name('mysql'), '-u', 'root'], input=sql, check=True)
    
def main():
    parser = argparse.ArgumentParser(description='Make MySQL run a .sql file')
    parser.add_argument('file_name')
    parser.add_argument('-s', '--skip-var', dest='skip')
    args = parser.parse_args()
    
    if args.skip is None or args.skip not in os.environ:
        run_sql_file(args.file_name)

if __name__ == '__main__':
    main()
