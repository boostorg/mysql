#!/usr/bin/python3
#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Utility to run the example TCP and HTTP servers

import os
import re
from subprocess import Popen, PIPE, STDOUT
from contextlib import contextmanager


_is_win = os.name == 'nt'


# Returns the port the server is listening at
def _parse_server_start_line(line: str) -> int:
    m = re.match(r'Server listening at 0\.0\.0\.0:([0-9]+)', line)
    if m is None:
        raise RuntimeError('Unexpected server start line')
    return int(m.group(1))


@contextmanager
def launch_server(exe: str, host: str, username: str, password: str):
    # Launch server and let it choose a free port for us.
    # This prevents port clashes during b2 parallel test runs
    server = Popen([exe, username, password, host, '0'], stdout=PIPE, stderr=STDOUT)
    assert server.stdout is not None
    with server:
        try:
            # Wait until the server is ready
            ready_line = server.stdout.readline().decode()
            print(ready_line, end='', flush=True)
            if ready_line.startswith('Sorry'): # C++ standard unsupported, skip the test
                exit(0)
            yield _parse_server_start_line(ready_line)
        finally:
            print('Terminating server...', flush=True)

            # In Windows, there is no sane way to cleanly terminate the process.
            # Sending a Ctrl-C terminates all process attached to the console (including ourselves
            # and any parent test runner). Running the process in a separate terminal doesn't allow
            # access to stdout, which is problematic, too.
            # terminate() sends SIGTERM in Unix, and uses TerminateProcess in Windows
            server.terminate()

            # Print any output the process generated
            print('Server stdout: \n', server.stdout.read().decode(), flush=True)

    # The return code is only relevant in Unix, as in Windows we used TerminateProcess
    if not _is_win and server.returncode != 0:
        raise RuntimeError('Server did not exit cleanly. retcode={}'.format(server.returncode))
