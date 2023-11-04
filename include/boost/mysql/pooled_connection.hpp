//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_POOLED_CONNECTION_HPP
#define BOOST_MYSQL_POOLED_CONNECTION_HPP

#include <boost/mysql/any_connection.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool/connection_node.hpp>

#include <memory>

namespace boost {
namespace mysql {

class pooled_connection
{
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

    pooled_connection(detail::connection_node& node) noexcept : impl_(&node) {}

    struct deleter
    {
        void operator()(detail::connection_node* node) const noexcept { node->mark_as_collectable(true); }
    };

    std::unique_ptr<detail::connection_node, deleter> impl_;

public:
    pooled_connection() noexcept = default;
    pooled_connection(const pooled_connection&) = delete;
    pooled_connection(pooled_connection&&) = default;
    pooled_connection& operator=(const pooled_connection&) = delete;
    pooled_connection& operator=(pooled_connection&&) = default;
    ~pooled_connection() = default;

    bool valid() const noexcept { return impl_.get() != nullptr; }

    any_connection& get() noexcept { return impl_->connection(); }
    const any_connection& get() const noexcept { return impl_->connection(); }
    any_connection* operator->() noexcept { return &get(); }
    const any_connection* operator->() const noexcept { return &get(); }

    void return_to_pool(bool should_reset = true) noexcept
    {
        impl_.release()->mark_as_collectable(should_reset);
    }
};

}  // namespace mysql
}  // namespace boost

#endif
