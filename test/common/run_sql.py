
from subprocess import run
import os
from sys import argv

def get_executable_name(name):
    if os.name == 'nt':
        name += '.exe'
    return name

def run_sql_file(fname):
    print('Running SQL file: {}'.format(fname))
    with open(fname, 'rb') as f:
        sql = f.read()
    run([get_executable_name('mysql'), '-u', 'root'], input=sql, check=True)

if __name__ == '__main__':
    run_sql_file(argv[1])
