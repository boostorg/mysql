//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_INTRUSIVE_LIST_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_INTRUSIVE_LIST_HPP

// Minimal functionality required to implement an intrusive list
// Based on Boost.Intrusive, but more lightweight

namespace boost {
namespace mysql {
namespace detail {

struct list_node
{
    list_node* prev{};
    list_node* next{};

    list_node(list_node* prev = nullptr, list_node* next = nullptr) noexcept : prev(prev), next(next) {}
};

template <class T>
class intrusive_list
{
    list_node head_;

public:
    intrusive_list() noexcept : head_(&head_, &head_) {}

    // Exposed for testing
    const list_node& head() const noexcept { return head_; }

    T* try_get_first() noexcept { return head_.next == &head_ ? nullptr : static_cast<T*>(head_.next); }

    void push_back(T& elm)
    {
        list_node& node = elm;
        list_node& prev = *head_.prev;

        BOOST_ASSERT(node.prev == nullptr);
        BOOST_ASSERT(node.next == nullptr);

        node.next = &head_;
        node.prev = &prev;
        head_.prev = &node;
        prev.next = &node;
    }

    void erase(T& elm)
    {
        list_node& node = elm;
        list_node* prev = node.prev;
        list_node* next = node.next;

        BOOST_ASSERT(prev != nullptr);
        BOOST_ASSERT(next != nullptr);

        prev->next = next;
        next->prev = prev;
        node.next = nullptr;
        node.prev = nullptr;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
