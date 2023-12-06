//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/connection_pool/intrusive_list.hpp>

#include <boost/test/unit_test.hpp>

#include <vector>

using namespace boost::mysql::detail;

BOOST_AUTO_TEST_SUITE(test_intrusive_list)

struct mock_node : list_node
{
    mock_node() = default;
};

using mock_list = intrusive_list<mock_node>;

void check(const mock_list& list, const std::vector<const mock_node*>& expected)
{
    std::vector<const mock_node*> actual;
    const auto& head = list.head();
    for (const list_node* node = head.next; node != &head; node = node->next)
        actual.push_back(static_cast<const mock_node*>(node));
    BOOST_TEST(actual == expected);
}

BOOST_AUTO_TEST_CASE(default_ctor)
{
    mock_list l;
    check(l, {});
    BOOST_TEST(l.try_get_first() == nullptr);
}

BOOST_AUTO_TEST_CASE(push_back)
{
    mock_list l;
    mock_node n1, n2, n3;

    // Add one
    l.push_back(n1);
    check(l, {&n1});
    BOOST_TEST(l.try_get_first() == &n1);

    // Adding another places it at the end
    l.push_back(n2);
    check(l, {&n1, &n2});
    BOOST_TEST(l.try_get_first() == &n1);

    // Same
    l.push_back(n3);
    check(l, {&n1, &n2, &n3});
    BOOST_TEST(l.try_get_first() == &n1);
}

BOOST_AUTO_TEST_CASE(erase)
{
    mock_list l;
    mock_node n1, n2, n3, n4;

    l.push_back(n1);
    l.push_back(n2);
    l.push_back(n3);
    l.push_back(n4);

    // Remove one in the middle
    l.erase(n2);
    check(l, {&n1, &n3, &n4});
    BOOST_TEST(l.try_get_first() == &n1);

    // Remove the first one
    l.erase(n1);
    check(l, {&n3, &n4});
    BOOST_TEST(l.try_get_first() == &n3);

    // Remove the last one
    l.erase(n4);
    check(l, {&n3});
    BOOST_TEST(l.try_get_first() == &n3);

    // Remove the only one remaining
    l.erase(n3);
    check(l, {});
    BOOST_TEST(l.try_get_first() == nullptr);
}

BOOST_AUTO_TEST_CASE(push_back_erase_interleaved)
{
    mock_list l;
    mock_node n1, n2, n3, n4;

    l.push_back(n1);
    l.push_back(n2);
    l.push_back(n3);

    // Remove one
    l.erase(n2);
    check(l, {&n1, &n3});
    BOOST_TEST(l.try_get_first() == &n1);

    // Add one
    l.push_back(n4);
    check(l, {&n1, &n3, &n4});
    BOOST_TEST(l.try_get_first() == &n1);

    // Remove the front one
    l.erase(n1);
    check(l, {&n3, &n4});
    BOOST_TEST(l.try_get_first() == &n3);

    // Add a node that has already been in the list
    l.push_back(n2);
    check(l, {&n3, &n4, &n2});
    BOOST_TEST(l.try_get_first() == &n3);

    // Add another one
    l.push_back(n1);
    check(l, {&n3, &n4, &n2, &n1});
    BOOST_TEST(l.try_get_first() == &n3);

    // Remove a node that has already been added and removed
    l.erase(n2);
    check(l, {&n3, &n4, &n1});
    BOOST_TEST(l.try_get_first() == &n3);
}

BOOST_AUTO_TEST_SUITE_END()
