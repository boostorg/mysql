#!/usr/bin/python3
#
# Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Waits for the database container to be ready for connections
# by polling the container log file

from subprocess import check_output, STDOUT
import time

READY_PHRASE = 'mysqld: ready for connections'
MAX_TRIES = 100
LOG_EACH = 10
BETWEEN_TRIES = 3.0
CONTAINER_NAME = 'mysql'

def is_ready():
    logs = check_output(['docker', 'logs', CONTAINER_NAME], stderr=STDOUT)
    return logs.decode().count(READY_PHRASE) >= 2

def main():
    print('Waiting for container to be ready')
    for i in range(1, MAX_TRIES + 1):
        if i % LOG_EACH == 0:
            print('Still waiting for container, try {}'.format(i))
        if is_ready():
            print('Container ready')
            exit(0)
        else:
            time.sleep(BETWEEN_TRIES)
    print('Failed to wait for container')
    exit(1)
    
if __name__ == '__main__':
    main()

