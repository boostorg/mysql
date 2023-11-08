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
#include <boost/mysql/detail/any_address.hpp>

#include <string>

namespace boost {
namespace mysql {

class connect_params
{
    struct
    {
        address_type addr_type{address_type::tcp_address};
        std::string address;
        unsigned short port{3306};
        std::string username;
        std::string password;
        std::string database;
        std::uint16_t connection_collation{handshake_params::default_collation};
        ssl_mode ssl{ssl_mode::require};
        bool multi_queries{};

        ssl_mode adjusted_ssl_mode() const noexcept
        {
            return addr_type == address_type::tcp_address ? ssl : ssl_mode::disable;
        }

        string_view adjusted_address() const noexcept
        {
            return addr_type == address_type::tcp_address && address.empty() ? string_view("localhost")
                                                                             : string_view(address);
        }

        detail::any_address to_address() const noexcept { return {addr_type, adjusted_address(), port}; }
        handshake_params to_handshake_params() const noexcept
        {
            return handshake_params(
                username,
                password,
                database,
                connection_collation,
                adjusted_ssl_mode(),
                multi_queries
            );
        }
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
        return impl_.adjusted_address();
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

    ssl_mode ssl() const noexcept { return impl_.adjusted_ssl_mode(); }

    bool multi_queries() const noexcept { return impl_.multi_queries; }

    // setters
    connect_params& set_tcp_address(std::string hostname, unsigned short port = 3306)
    {
        impl_.addr_type = address_type::tcp_address;
        impl_.address = std::move(hostname);
        impl_.port = port;
        return *this;
    }

    connect_params& set_unix_address(std::string path)
    {
        impl_.addr_type = address_type::unix_path;
        impl_.address = std::move(path);
        return *this;
    }

    connect_params& set_username(std::string val)
    {
        impl_.username = std::move(val);
        return *this;
    }

    connect_params& set_password(std::string passwd)
    {
        impl_.password = std::move(passwd);
        return *this;
    }

    connect_params& set_database(std::string val)
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
