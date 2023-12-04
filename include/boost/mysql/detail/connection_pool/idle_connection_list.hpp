//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_IDLE_CONNECTION_LIST_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_IDLE_CONNECTION_LIST_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/intrusive/list.hpp>

namespace boost {
namespace mysql {
namespace detail {

// TODO: we may want to get rid of intrusive to decrease build times
using hook_type = intrusive::list_base_hook<>;

class connection_node;

class idle_connection_list
{
    intrusive::list<connection_node, intrusive::base_hook<hook_type>> list_;
    asio::experimental::channel<void(error_code)> chan_;  // TODO: could we make these void()?
    error_code last_ec_;
    diagnostics last_diag_;

public:
    idle_connection_list(boost::asio::any_io_executor ex) : chan_(ex, 1) {}

    boost::asio::any_io_executor get_executor() { return chan_.get_executor(); }

    connection_node* try_get_one() noexcept { return list_.empty() ? nullptr : &list_.back(); }

    template <class CompletionToken>
    auto async_wait(CompletionToken&& token)
        -> decltype(chan_.async_receive(std::forward<CompletionToken>(token)))
    {
        return chan_.async_receive(std::forward<CompletionToken>(token));
    }

    void add_one(connection_node& node)
    {
        list_.push_back(node);
        chan_.try_send(error_code());
    }

    void close_channel() { chan_.close(); }

    void remove(connection_node& node) { list_.erase(list_.iterator_to(node)); }

    std::size_t size() const noexcept { return list_.size(); }

    void set_last_error(error_code ec, diagnostics&& diag)
    {
        last_ec_ = ec;
        last_diag_ = std::move(diag);
    }

    error_code last_error() const noexcept { return last_ec_; }
    const diagnostics& last_diagnostics() const noexcept { return last_diag_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
