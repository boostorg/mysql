from subprocess import run
import os
from os import path, environ

def get_executable_name(name):
    if os.name == 'nt':
        name += '.exe'
    return name

def run_sql_file(fname):
    print('Running SQL setup file: {}'.format(fname))
    with open(fname, 'rb') as f:
        sql = f.read()
    run([get_executable_name('mysql'), '-u', 'root'], input=sql)

def main():
    has_sha256 = 'MYSQL_HAS_SHA256' in environ
    this_file_dir = path.dirname(path.abspath(__file__))
    
    # Run setup
    if not 'MYSQL_SKIP_DB_SETUP' in environ:
        run_sql_file(path.join(this_file_dir, 'db_setup.sql'))
    
    # Path to the test binary
    test_exe = path.join(os.getcwd(), 'mysql_integrationtests')
    if os.name == 'nt':
        test_exe += '.exe'
    
    # Run tests
    if 'MYSQL_HAS_SHA256' in environ:
        run_sql_file(path.join(this_file_dir, 'db_setup_sha256.sql'))
        print('Running integration tests with SHA256 support')
        run([test_exe])
    else:
        print('Running integration tests without SHA256 support')
        run([test_exe, '--gtest_filter=-*RequiresSha256*']) # Exclude anything containing RequiresSha256

if __name__ == '__main__':
    main()
