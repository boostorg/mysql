#!/usr/bin/python3
#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

import argparse
import sys
from os import path
import socket
import struct

sys.path.append(path.abspath(path.dirname(path.realpath(__file__))))
from launch_server import launch_server


class _Runner:
    def __init__(self, port: int) -> None:
        self._port = port
    
    def _connect(self) -> socket.socket:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(('127.0.0.1', self._port))
        return sock


    def _query_employee(self, employee_id: int) -> str:
        # Open a connection
        sock = self._connect()

        # Send the request
        sock.send(struct.pack('>Q', employee_id))
        
        # Receive the response. It should always fit in a single TCP segment
        # for the values we have in CI
        res = sock.recv(4096).decode()
        assert len(res) > 0
        return res


    def _generate_error(self) -> None:
        # Open a connection
        sock = self._connect()

        # Send an incomplete message
        sock.send(b'abc')
        sock.close()
    

    def run(self, test_errors: bool) -> None:
        # Generate an error first. The server should not terminate
        if test_errors:
            self._generate_error()
        assert self._query_employee(1) != 'NOT_FOUND'
        value = self._query_employee(0xffffffff)
        assert value == 'NOT_FOUND', 'Value is: {}'.format(value)


def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('host')
    parser.add_argument('--test-errors', action='store_true')
    args = parser.parse_args()

    # Launch the server
    with launch_server(args.executable, args.host, 'example_user', 'example_password') as listening_port:
    # Run the tests
        _Runner(listening_port).run(args.test_errors)


if __name__ == '__main__':
    main()
