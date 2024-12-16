#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

import requests
import argparse
from subprocess import PIPE, STDOUT, Popen
from contextlib import contextmanager
import re
import os
import unittest
import copy

_is_win = os.name == 'nt'


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
    server = Popen([exe, 'orders_user', 'orders_password', host, '0'], stdout=PIPE, stderr=STDOUT)
    assert server.stdout is not None
    with server:
        try:
            # Wait until the server is ready
            ready_line = server.stdout.readline().decode()
            print(ready_line, end='', flush=True)
            if ready_line.startswith('Sorry'): # C++20 unsupported, skip the test
                exit(0)
            yield _parse_server_start_line(ready_line)
        finally:
            print('Terminating server...', flush=True)

            # In Windows, there is no sane way to cleanly terminate the process.
            # Sending a Ctrl-C terminates all process attached to the console (including ourselves
            # and any parent test runner). Running the process in a separate terminal doesn't allow
            # access to stdout, which is problematic, too.
            if _is_win:
                # kill is an alias for TerminateProcess with the given exit code
                os.kill(server.pid, 9999)
            else:
                # Send SIGTERM
                server.terminate()

            # Print any output the process generated
            print('Server stdout: \n', server.stdout.read().decode(), flush=True)
    
    # Verify that it exited gracefully
    if (_is_win and server.returncode != 9999) or (not _is_win and server.returncode):
        raise RuntimeError('Server did not exit cleanly. retcode={}'.format(server.returncode))


class TestOrders(unittest.TestCase):

    def __init__(self, method_name: str, port: int) -> None:
        super().__init__(method_name)
        self._base_url = 'http://127.0.0.1:{}'.format(port)
    
    @staticmethod
    def _json_response(res: requests.Response):
        if res.status_code >= 400:
            print(res.text)
        res.raise_for_status()
        return res.json()


    def _check_error(self, res: requests.Response, expected_status: int) -> None:
        self.assertEqual(res.status_code, expected_status)
    

    def _request(self, method: str, url: str, **kwargs) -> requests.Response:
        return requests.request(method=method, url=self._base_url + url, **kwargs)


    def _request_as_json(self, method: str, url: str, **kwargs):
        return self._json_response(self._request(method, url, **kwargs))


    def test_search_products(self) -> None:
        # Issue the request
        products = self._request_as_json('get', '/products', params={'search': 'odin'})

        # Check
        self.assertNotEqual(len(products), 0) # At least one product
        odin = products[0]
        self.assertIsInstance(odin['id'], int) # We don't know the exact ID
        self.assertEqual(odin['short_name'], 'A Feast for Odin')
        self.assertEqual(odin['price'], 6400)
        self.assertIsInstance(odin['descr'], str)
    

    def test_search_products_missing_param(self) -> None:
        self._check_error(self._request('get', '/products'), 400)
    

    def test_order_lifecycle(self) -> None:
        # Create an order
        order = self._request_as_json('post', '/orders')
        order_id: int = order['id']
        self.assertIsInstance(order_id, int)
        self.assertEqual(order['status'], 'draft')
        self.assertEqual(order['items'], [])

        # Add an item
        order = self._request_as_json('post', '/orders/items', json={
            'order_id': order_id,
            'product_id': 2,
            'quantity': 20
        })
        items = order['items']
        self.assertEqual(order['id'], order_id)
        self.assertEqual(order['status'], 'draft')
        self.assertEqual(len(order['items']), 1)
        self.assertIsInstance(items[0]['id'], int)
        self.assertEqual(items[0]['product_id'], 2)
        self.assertEqual(items[0]['quantity'], 20)

        # Checkout
        expected_order = copy.deepcopy(order)
        expected_order['status'] = 'pending_payment'
        order = self._request_as_json('post', '/orders/checkout', params={'id': order_id})
        self.assertEqual(order, expected_order)

        # Complete
        expected_order = copy.deepcopy(order)
        expected_order['status'] = 'complete'
        order = self._request_as_json('post', '/orders/complete', params={'id': order_id})
        self.assertEqual(order, expected_order)
    

    def test_remove_items(self) -> None:
        # Create an order
        order1 = self._request_as_json('post', '/orders')
        order_id: int = order1['id']

        # Create two items
        self._request_as_json('post', '/orders/items', json={
            'order_id': order_id,
            'product_id': 2,
            'quantity': 20
        })
        order2 = self._request_as_json('post', '/orders/items', json={
            'order_id': order_id,
            'product_id': 1,
            'quantity': 1
        })
        order2_items = order2['items']

        # Sanity check
        self.assertEqual(order2['id'], order_id)
        self.assertEqual(order2['status'], 'draft')
        self.assertEqual(len(order2_items), 2)
        product_ids = list(set(item['product_id'] for item in order2_items))
        self.assertEqual(product_ids, [1, 2]) # IDs 1 and 2 in any order

        # Delete one of the items
        order3 = self._request_as_json('delete', '/orders/items', params={'id': order2_items[0]['id']})
        self.assertEqual(order3['id'], order_id)
        self.assertEqual(order3['status'], 'draft')
        self.assertEqual(order3['items'], [order2_items[1]])
    

    def test_get_orders(self) -> None:
        orders = self._request_as_json('get', '/orders')
        self.assertIsInstance(orders, list)
    

    def test_get_single_order(self) -> None:
        # Create an order and add an item
        order = self._request_as_json('post', '/orders')
        order = self._request_as_json('post', '/orders/items', json={
            'order_id': order['id'],
            'product_id': 2,
            'quantity': 20
        })

        # Retrieve the order by id
        order2 = self._request_as_json('get', '/orders', params={'id': order['id']})
        self.assertEqual(order2, order)




    

    # Method not allowed
    # Endpoint not found
    # Invalid content-type


# def _call_endpoints(port: int):
#     base_url = 'http://127.0.0.1:{}'.format(port)

#     # Search products
#     # List orders (empty)
#     # Create an order
#     # Retrieve the order
#     # Add some line items
#     # Retrieve the order
#     # Remove a line item
#     # Checkout the order
#     # Complete the order

#     # Get an order that doesn't exist
#     # Add a line item to an order that doesn't exist
#     # Add a line item for an order that doesn't exist

#     # Create a note
#     note_unique = _random_string()
#     title = 'My note {}'.format(note_unique)
#     content = 'This is a note about {}'.format(note_unique)
#     res = requests.post(
#         '{}/notes'.format(base_url),
#         json={'title': title, 'content': content}
#     )
#     _check_response(res)
#     note = res.json()
#     note_id = int(note['note']['id'])
#     assert note['note']['title'] == title
#     assert note['note']['content'] == content

#     # Retrieve all notes
#     res = requests.get('{}/notes'.format(base_url))
#     _check_response(res)
#     all_notes = res.json()
#     assert len([n for n in all_notes['notes'] if n['id'] == note_id]) == 1

#     # Edit the note
#     note_unique = _random_string()
#     title = 'Edited {}'.format(note_unique)
#     content = 'This is a note an edit on {}'.format(note_unique)
#     res = requests.put(
#         '{}/notes/{}'.format(base_url, note_id),
#         json={'title': title, 'content': content}
#     )
#     _check_response(res)
#     note = res.json()
#     assert int(note['note']['id']) == note_id
#     assert note['note']['title'] == title
#     assert note['note']['content'] == content

#     # Retrieve the note
#     res = requests.get('{}/notes/{}'.format(base_url, note_id))
#     _check_response(res)
#     note = res.json()
#     assert int(note['note']['id']) == note_id
#     assert note['note']['title'] == title
#     assert note['note']['content'] == content

#     # Delete the note
#     res = requests.delete('{}/notes/{}'.format(base_url, note_id))
#     _check_response(res)
#     assert res.json()['deleted'] == True

#     # The note is not there
#     res = requests.get('{}/notes/{}'.format(base_url, note_id))
#     assert res.status_code == 404


def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('host')
    args = parser.parse_args()

    # Launch the server
    with _launch_server(args.executable, args.host) as listening_port:
        tests = [
            TestOrders(method, listening_port)
            for method in unittest.defaultTestLoader.getTestCaseNames(TestOrders)
        ]
        suite = unittest.TestSuite()
        suite.addTests(tests)
        unittest.TextTestRunner().run(suite)


if __name__ == '__main__':
    main()
