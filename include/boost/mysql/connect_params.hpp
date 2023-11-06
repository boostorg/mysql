//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECT_PARAMS_HPP
#define BOOST_MYSQL_CONNECT_PARAMS_HPP

#include <boost/mysql/address_type.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>

namespace boost {
namespace mysql {

class connect_params
{
    struct
    {
        address_type addr_type{address_type::tcp_address};
        string_view address{"localhost"};
        unsigned short port{3306};
        string_view username;
        string_view password;
        string_view database;
        std::uint16_t connection_collation{handshake_params::default_collation};
        ssl_mode ssl{ssl_mode::require};
        bool multi_queries{};
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    connect_params() = default;

    // getters
    address_type addr_type() const noexcept { return impl_.addr_type; }

    string_view hostname() const noexcept
    {
        BOOST_ASSERT(impl_.addr_type == address_type::tcp_address);
        return impl_.address;
    }

    unsigned short port() const noexcept
    {
        BOOST_ASSERT(impl_.addr_type == address_type::tcp_address);
        return impl_.port;
    }

    string_view unix_path() const noexcept
    {
        BOOST_ASSERT(impl_.addr_type == address_type::unix_path);
        return impl_.address;
    }

    string_view username() const noexcept { return impl_.username; }

    string_view password() const noexcept { return impl_.password; }

    string_view database() const noexcept { return impl_.database; }

    std::uint16_t connection_collation() const noexcept { return impl_.connection_collation; }

    ssl_mode ssl() const noexcept
    {
        return impl_.addr_type == address_type::tcp_address ? impl_.ssl : ssl_mode::disable;
    }

    bool multi_queries() const noexcept { return impl_.multi_queries; }

    // setters
    connect_params& set_tcp_address(string_view hostname, unsigned short port = 3306) noexcept
    {
        impl_.addr_type = address_type::tcp_address;
        impl_.address = hostname;
        impl_.port = port;
        return *this;
    }

    connect_params& set_unix_address(string_view path) noexcept
    {
        impl_.addr_type = address_type::unix_path;
        impl_.address = path;
        return *this;
    }

    connect_params& set_username(string_view val) noexcept
    {
        impl_.username = val;
        return *this;
    }

    connect_params& set_password(string_view passwd) noexcept
    {
        impl_.password = std::move(passwd);
        return *this;
    }

    connect_params& set_database(string_view val) noexcept
    {
        impl_.database = std::move(val);
        return *this;
    }

    connect_params& set_connection_collation(std::uint16_t val) noexcept
    {
        impl_.connection_collation = val;
        return *this;
    }

    connect_params& set_ssl(ssl_mode ssl) noexcept
    {
        impl_.ssl = ssl;
        return *this;
    }

    connect_params& set_multi_queries(bool val) noexcept
    {
        impl_.multi_queries = val;
        return *this;
    }
};

}  // namespace mysql
}  // namespace boost

#endif
