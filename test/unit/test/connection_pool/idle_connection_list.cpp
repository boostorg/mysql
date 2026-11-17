//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/connection_pool/idle_connection_list.hpp>

#include <boost/test/unit_test.hpp>

#include <vector>

using namespace boost::mysql;
using boost::test_tools::per_element;
using detail::idle_connection_list;
using detail::idle_list_hook;

BOOST_AUTO_TEST_SUITE(test_idle_connection_list)

// Tests declare the nodes before the list, so the list is destroyed first.
// Its destructor unlinks the nodes that are still in it. A node destroyed
// while linked would trigger an assertion
struct mock_node : idle_list_hook
{
    int id;

    explicit mock_node(int node_id) noexcept : id(node_id) {}
};

// The ids of the elements, from the least to the most recently returned
std::vector<int> get_ids(const idle_connection_list<mock_node>& list)
{
    std::vector<int> res;
    for (const mock_node& node : list)
    {
        res.push_back(node.id);
    }
    return res;
}

BOOST_AUTO_TEST_CASE(empty_by_default)
{
    idle_connection_list<mock_node> list;

    BOOST_TEST(list.empty());
    BOOST_TEST(list.size() == 0u);
}

BOOST_AUTO_TEST_CASE(push_back_most_recent_last)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;

    // Elements are returned one after the other
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // The last one is the most recently returned
    BOOST_TEST(!list.empty());
    BOOST_TEST(list.size() == 3u);
    BOOST_TEST(list.back().id == 3);
    const std::vector<int> expected{1, 2, 3};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(push_back_after_erase_becomes_most_recent)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // The least recently returned element is handed out and returned
    list.erase(node1);
    list.push_back(node1);

    // It's now the most recently returned
    BOOST_TEST(list.back().id == 1);
    const std::vector<int> expected{2, 3, 1};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(erase_most_recent)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // The most recently returned element is removed
    list.erase(node3);

    // The one returned before it becomes the most recently returned
    BOOST_TEST(list.size() == 2u);
    BOOST_TEST(list.back().id == 2);
    const std::vector<int> expected{1, 2};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(erase_middle)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // An element that isn't the most recently returned is removed
    list.erase(node2);

    // The others keep their order
    BOOST_TEST(list.size() == 2u);
    const std::vector<int> expected{1, 3};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

// Restoring an element puts it back in its place, wherever that is
BOOST_AUTO_TEST_CASE(restore_least_recent)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // The least recently returned element leaves the list and gets back
    list.erase(node1);
    list.restore(node1);

    // It's still the least recently returned
    BOOST_TEST(list.size() == 3u);
    const std::vector<int> expected{1, 2, 3};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(restore_middle)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // An element in the middle leaves the list and gets back
    list.erase(node2);
    list.restore(node2);

    // It's still in the middle
    const std::vector<int> expected{1, 2, 3};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(restore_most_recent)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // The most recently returned element leaves the list and gets back
    list.erase(node3);
    list.restore(node3);

    // It's still the most recently returned
    BOOST_TEST(list.back().id == 3);
    const std::vector<int> expected{1, 2, 3};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(restore_repeatedly)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // An element leaves the list and gets back several times, as periodic pings do
    list.erase(node2);
    list.restore(node2);
    list.erase(node2);
    list.restore(node2);

    // It's still in its place
    const std::vector<int> expected{1, 2, 3};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(restore_after_push_back)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);

    // The most recently returned element leaves the list.
    // Another element is returned before it gets back
    list.erase(node2);
    list.push_back(node3);
    list.restore(node2);

    // The element returned in the meanwhile is the most recently returned
    BOOST_TEST(list.back().id == 3);
    const std::vector<int> expected{1, 2, 3};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(restore_after_neighbours_returned)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // An element leaves the list. Its neighbours are handed out and returned before it gets back
    list.erase(node2);
    list.erase(node3);
    list.erase(node1);
    list.push_back(node3);
    list.push_back(node1);
    list.restore(node2);

    // Both were returned after it
    const std::vector<int> expected{2, 3, 1};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(restore_into_empty_list)
{
    // Setup
    mock_node node1(1), node2(2), node3(3);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);

    // An element leaves the list. The others are removed before it gets back
    list.erase(node2);
    list.erase(node1);
    list.erase(node3);
    list.restore(node2);

    // It's the only element
    BOOST_TEST(list.size() == 1u);
    const std::vector<int> expected{2};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(restore_several)
{
    // Setup
    mock_node node1(1), node2(2), node3(3), node4(4);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);
    list.push_back(node4);

    // Two elements are out of the list at the same time, and get back in the order they left
    list.erase(node2);
    list.erase(node3);
    list.restore(node2);
    list.restore(node3);

    // Both are in their places
    const std::vector<int> expected{1, 2, 3, 4};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_CASE(restore_several_reverse_order)
{
    // Setup
    mock_node node1(1), node2(2), node3(3), node4(4);
    idle_connection_list<mock_node> list;
    list.push_back(node1);
    list.push_back(node2);
    list.push_back(node3);
    list.push_back(node4);

    // Two elements are out of the list at the same time, and get back in reverse order
    list.erase(node2);
    list.erase(node3);
    list.restore(node3);
    list.restore(node2);

    // Both are in their places
    const std::vector<int> expected{1, 2, 3, 4};
    BOOST_TEST(get_ids(list) == expected, per_element());
}

BOOST_AUTO_TEST_SUITE_END()
