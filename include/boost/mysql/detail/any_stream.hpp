//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ANY_STREAM_HPP
#define BOOST_MYSQL_DETAIL_ANY_STREAM_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

class any_stream
{
    bool ssl_active_{false};

public:
    void reset() noexcept { set_ssl_active(false); }  // TODO: do we really need this?
    bool ssl_active() const noexcept { return ssl_active_; }
    void set_ssl_active(bool v) noexcept { ssl_active_ = v; }

    using executor_type = boost::asio::any_io_executor;
    using lowest_layer_type = any_stream;

    virtual ~any_stream() {}

    virtual executor_type get_executor() = 0;

    // SSL
    virtual bool supports_ssl() const noexcept = 0;
    virtual void handshake(error_code& ec) = 0;
    virtual void async_handshake(asio::any_completion_handler<void(error_code)>) = 0;
    virtual void shutdown(error_code& ec) = 0;
    virtual void async_shutdown(asio::any_completion_handler<void(error_code)>) = 0;

    // Reading
    virtual std::size_t read_some(boost::asio::mutable_buffer, error_code& ec) = 0;
    virtual void async_read_some(boost::asio::mutable_buffer, asio::any_completion_handler<void(error_code, std::size_t)>) = 0;

    // Writing
    virtual std::size_t write_some(boost::asio::const_buffer, error_code& ec) = 0;
    virtual void async_write_some(boost::asio::const_buffer, asio::any_completion_handler<void(error_code, std::size_t)>) = 0;

    // Connect and close (TODO: is there a better way?)
    virtual void connect(const void* endpoint, error_code& ec) = 0;
    virtual void async_connect(const void* endpoint, asio::any_completion_handler<void(error_code)>) = 0;
    virtual void close(error_code& ec) = 0;
    virtual bool is_open() const noexcept = 0;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
