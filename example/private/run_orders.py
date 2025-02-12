#!/usr/bin/python3
#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

import requests
import argparse
from os import path
import unittest
import copy
import sys

sys.path.append(path.abspath(path.dirname(path.realpath(__file__))))
from launch_server import launch_server


class TestOrders(unittest.TestCase):
    _port = -1

    @property
    def _base_url(self) -> str:
        return 'http://127.0.0.1:{}'.format(self._port)
    
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
    

    def _request_error(self, method: str, url: str, expected_status: int, **kwargs):
        return self._check_error(self._request(method, url, **kwargs), expected_status)

    #
    # Success cases 
    #
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
    

    def test_order_lifecycle(self) -> None:
        # Create an order
        order = self._request_as_json('post', '/orders')
        order_id = order['id']
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
        order_id = order1['id']

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
    

    #
    # Search products errors
    #
    def test_search_products_missing_param(self) -> None:
        self._request_error('get', '/products', expected_status=400)
    

    #
    # Get order errors
    #
    def test_get_order_invalid_id(self) -> None:
        self._request_error('get', '/orders', params={'id': 'abc'}, expected_status=400)
    

    def test_get_order_not_found(self) -> None:
        self._request_error('get', '/orders', params={'id': 0xffffff}, expected_status=404)


    #
    # Add order item errors
    #
    def test_add_order_item_invalid_content_type(self) -> None:
        # Create an order
        order_id = self._request_as_json('post', '/orders')['id']
        
        # Check the error
        self._request_error('post', '/orders/items', headers={'Content-Type':'text/html'}, json={
            'order_id': order_id,
            'product_id': 1,
            'quantity': 1
        }, expected_status=400)


    def test_add_order_item_invalid_json(self) -> None:
        self._request_error('post', '/orders/items', headers={'Content-Type':'application/json'},
                            data='bad', expected_status=400)


    def test_add_order_item_invalid_json_keys(self) -> None:
        self._request_error('post', '/orders/items', json={
            'order_id': '1',
            'product_id': 1,
            'quantity': 1
        }, expected_status=400)
    

    def test_add_order_item_order_not_found(self) -> None:
        self._request_error('post', '/orders/items', json={
            'order_id': 0xffffffff,
            'product_id': 1,
            'quantity': 1
        }, expected_status=404)
    

    def test_add_order_item_product_not_found(self) -> None:
        # Create an order
        order_id = self._request_as_json('post', '/orders')['id']

        # Check the error
        self._request_error('post', '/orders/items', json={
            'order_id': order_id,
            'product_id': 0xffffffff,
            'quantity': 1
        }, expected_status=422)
    

    def test_add_order_item_order_not_editable(self) -> None:
        # Create an order and check it out
        order_id = self._request_as_json('post', '/orders')['id']
        self._request_as_json('post', '/orders/checkout', params={'id': order_id})

        # Check the error
        self._request_error('post', '/orders/items', json={
            'order_id': order_id,
            'product_id': 1,
            'quantity': 1
        }, expected_status=422)

    #
    # Remove order item errors
    #
    def test_remove_order_item_missing_id(self) -> None:
        self._request_error('delete', '/orders/items', expected_status=400)


    def test_remove_order_item_invalid_id(self) -> None:
        self._request_error('delete', '/orders/items', params={'id': 'abc'}, expected_status=400)


    def test_remove_order_item_not_found(self) -> None:
        self._request_error('delete', '/orders/items', params={'id': 0xffffffff}, expected_status=404)


    def test_remove_order_item_order_not_editable(self) -> None:
        # Create an order with an item and check it out
        order_id = self._request_as_json('post', '/orders')['id']
        item_id = self._request_as_json('post', '/orders/items', json={
            'order_id': order_id,
            'product_id': 2,
            'quantity': 20
        })['items'][0]['id']
        self._request_as_json('post', '/orders/checkout', params={'id': order_id})

        # Check the error
        self._request_error('delete', '/orders/items', params={'id': item_id}, expected_status=422)
    

    #
    # Checkout order errors
    #
    def test_checkout_order_missing_id(self) -> None:
        self._request_error('post', '/orders/checkout', expected_status=400)


    def test_checkout_order_invalid_id(self) -> None:
        self._request_error('post', '/orders/checkout', params={'id': 'abc'}, expected_status=400)


    def test_checkout_order_not_found(self) -> None:
        self._request_error('post', '/orders/checkout', params={'id': 0xffffffff}, expected_status=404)
    

    def test_checkout_order_not_editable(self) -> None:
        # Create an order and check it out
        order_id = self._request_as_json('post', '/orders')['id']
        self._request_as_json('post', '/orders/checkout', params={'id': order_id})

        # Check the error
        self._request_error('post', '/orders/checkout', params={'id': order_id}, expected_status=422)


    #
    # Complete order errors
    #
    def test_complete_order_missing_id(self) -> None:
        self._request_error('post', '/orders/complete', expected_status=400)


    def test_complete_order_invalid_id(self) -> None:
        self._request_error('post', '/orders/complete', params={'id': 'abc'}, expected_status=400)


    def test_complete_order_not_found(self) -> None:
        self._request_error('post', '/orders/complete', params={'id': 0xffffffff}, expected_status=404)
    

    def test_complete_order_not_editable(self) -> None:
        # Create an order
        order_id = self._request_as_json('post', '/orders')['id']

        # Check the error
        self._request_error('post', '/orders/complete', params={'id': order_id}, expected_status=422)
    

    #
    # Generic errors
    #
    
    def test_endpoint_not_found(self) -> None:
        self._request_error('get', '/orders/other', expected_status=404)
        self._request_error('get', '/orders_other', expected_status=404)
    

    def test_method_not_allowed(self) -> None:
        self._request_error('delete', '/orders', expected_status=405)
        

def main():
    # Parse command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('host')
    args = parser.parse_args()

    # Launch the server
    with launch_server(args.executable, args.host, 'orders_user', 'orders_password') as listening_port:
        TestOrders._port = listening_port
        unittest.main(argv=[sys.argv[0]])


if __name__ == '__main__':
    main()
