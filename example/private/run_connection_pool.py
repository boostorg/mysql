#!/usr/bin/python3
#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

import requests
import random
import argparse
from subprocess import PIPE, Popen
from contextlib import contextmanager
import re


def _check_response(res: requests.Response):
    if res.status_code >= 400:
        print(res.text)
    res.raise_for_status()


def _random_string() -> str:
    return bytes(random.getrandbits(8) for _ in range(8)).hex()


# Returns the port the server is listening at
def _parse_server_start_line(line: str) -> int:
    m = re.match(r'Server listening at 0\.0\.0\.0:([0-9]+)', line)
    if m is None:
        raise RuntimeError('Unexpected server start line')
    return int(m.group(1))


@contextmanager
def _launch_server(exe: str, host: str):
    # Launch server and let it choose a free port for us.
    # This prevents port clashes during b2 parallel test runs
    server = Popen([exe, 'example_user', 'example_password', host, '0'], stdout=PIPE, stderr=PIPE)
    assert server.stdout is not None
    assert server.stderr is not None 
    with server:
        try:
            # Wait until the server is ready
            ready_line = server.stdout.readline().decode()
            print(ready_line, end='')
            yield _parse_server_start_line(ready_line)
        finally:
            # Send SIGTERM
            server.terminate()

            # Print any output the process generated
            print(server.stdout.read().decode(), end='')
            print(server.stderr.read().decode(), end='')
    
    # Verify that it exited gracefully
    if server.returncode:
        raise RuntimeError('Server did not exit cleanly. retcode={}'.format(server.returncode))


def _call_endpoints(port: int):
    base_url = 'http://localhost:{}'.format(port)

    # Create a note
    note_unique = _random_string()
    title = 'My note {}'.format(note_unique)
    content = 'This is a note about {}'.format(note_unique)
    res = requests.post(
        '{}/notes'.format(base_url),
        json={'title': title, 'content': content}
    )
    _check_response(res)
    note = res.json()
    note_id = int(note['note']['id'])
    assert note['note']['title'] == title
    assert note['note']['content'] == content

    # Retrieve all notes
    res = requests.get('{}/notes'.format(base_url))
    _check_response(res)
    all_notes = res.json()
    assert len([n for n in all_notes['notes'] if n['id'] == note_id]) == 1

    # Edit the note
    note_unique = _random_string()
    title = 'Edited {}'.format(note_unique)
    content = 'This is a note an edit on {}'.format(note_unique)
    res = requests.put(
        '{}/notes/{}'.format(base_url, note_id),
        json={'title': title, 'content': content}
    )
    _check_response(res)
    note = res.json()
    assert int(note['note']['id']) == note_id
    assert note['note']['title'] == title
    assert note['note']['content'] == content

    # Retrieve the note
    res = requests.get('{}/notes/{}'.format(base_url, note_id))
    _check_response(res)
    note = res.json()
    assert int(note['note']['id']) == note_id
    assert note['note']['title'] == title
    assert note['note']['content'] == content

    # Delete the note
    res = requests.delete('{}/notes/{}'.format(base_url, note_id))
    _check_response(res)
    assert res.json()['deleted'] == True

    # The note is not there
    res = requests.get('{}/notes/{}'.format(base_url, note_id))
    assert res.status_code == 404


def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('host')
    args = parser.parse_args()

    # Launch the server
    with _launch_server(args.executable, args.host) as listening_port:
        # Run the tests
        _call_endpoints(listening_port)


if __name__ == '__main__':
    main()
