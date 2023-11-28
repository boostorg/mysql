#!/usr/bin/python3
#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

import requests
import random

_BASE_URL = 'http://localhost:4000'

def _check_response(res: requests.Response):
    if res.status_code >= 400:
        print(res.text)
    res.raise_for_status()


def _random_string() -> str:
    return bytes(random.getrandbits(8) for _ in range(8)).hex()


def main():

    # Create a note
    note_unique = _random_string()
    title = 'My note {}'.format(note_unique)
    content = 'This is a note about {}'.format(note_unique)
    res = requests.post(
        '{}/notes'.format(_BASE_URL),
        json={'title': title, 'content': content}
    )
    _check_response(res)
    note = res.json()
    note_id = int(note['note']['id'])
    assert note['note']['title'] == title
    assert note['note']['content'] == content

    # Retrieve all notes
    res = requests.get('{}/notes'.format(_BASE_URL))
    _check_response(res)
    all_notes = res.json()
    assert len([n for n in all_notes['notes'] if n['id'] == note_id]) == 1

    # Edit the note
    note_unique = _random_string()
    title = 'Edited {}'.format(note_unique)
    content = 'This is a note an edit on {}'.format(note_unique)
    res = requests.put(
        '{}/notes/{}'.format(_BASE_URL, note_id),
        json={'title': title, 'content': content}
    )
    _check_response(res)
    note = res.json()
    assert int(note['note']['id']) == note_id
    assert note['note']['title'] == title
    assert note['note']['content'] == content

    # Retrieve the note
    res = requests.get('{}/notes/{}'.format(_BASE_URL, note_id))
    _check_response(res)
    note = res.json()
    assert int(note['note']['id']) == note_id
    assert note['note']['title'] == title
    assert note['note']['content'] == content

    # Delete the note
    res = requests.delete('{}/notes/{}'.format(_BASE_URL, note_id))
    _check_response(res)
    assert res.json()['deleted'] == True

    # The note is not there
    res = requests.get('{}/notes/{}'.format(_BASE_URL, note_id))
    assert res.status_code == 404


if __name__ == '__main__':
    main()
