#!/usr/bin/python3
#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

import requests
import random
import argparse
import sys
from os import path

sys.path.append(path.abspath(path.dirname(path.realpath(__file__))))
from launch_server import launch_server


def _check_response(res: requests.Response):
    if res.status_code >= 400:
        print(res.text)
    res.raise_for_status()


def _random_string() -> str:
    return bytes(random.getrandbits(8) for _ in range(8)).hex()


def _call_endpoints(port: int):
    url = 'http://127.0.0.1:{}/notes'.format(port)

    # Create a note
    note_unique = _random_string()
    title = 'My note {}'.format(note_unique)
    content = 'This is a note about {}'.format(note_unique)
    res = requests.post(
        url,
        json={'title': title, 'content': content}
    )
    _check_response(res)
    note = res.json()
    note_id = int(note['note']['id'])
    assert note['note']['title'] == title
    assert note['note']['content'] == content

    # Retrieve all notes
    res = requests.get(url)
    _check_response(res)
    all_notes = res.json()
    assert len([n for n in all_notes['notes'] if n['id'] == note_id]) == 1

    # Edit the note
    note_unique = _random_string()
    title = 'Edited {}'.format(note_unique)
    content = 'This is a note an edit on {}'.format(note_unique)
    res = requests.put(
        url,
        params={'id': note_id},
        json={'title': title, 'content': content}
    )
    _check_response(res)
    note = res.json()
    assert int(note['note']['id']) == note_id
    assert note['note']['title'] == title
    assert note['note']['content'] == content

    # Retrieve the note
    res = requests.get(url, params={'id': note_id})
    _check_response(res)
    note = res.json()
    assert int(note['note']['id']) == note_id
    assert note['note']['title'] == title
    assert note['note']['content'] == content

    # Delete the note
    res = requests.delete(url, params={'id': note_id})
    _check_response(res)
    assert res.json()['deleted'] == True

    # The note is not there
    res = requests.get(url, params={'id': note_id})
    assert res.status_code == 404


def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('host')
    args = parser.parse_args()

    # Launch the server
    with launch_server(args.executable, args.host, 'example_user', 'example_password') as listening_port:
    # Run the tests
        _call_endpoints(listening_port)


if __name__ == '__main__':
    main()
