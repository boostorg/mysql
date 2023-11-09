//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ANY_ADDRESS_HPP
#define BOOST_MYSQL_ANY_ADDRESS_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>

#include <string>

namespace boost {
namespace mysql {

enum class address_type
{
    tcp_address,
    unix_path
};

class any_address
{
    struct
    {
        address_type type{address_type::tcp_address};
        std::string address;
        unsigned short port{3306};
    } impl_;

    any_address(address_type t, std::string&& addr, unsigned short port) noexcept
        : impl_{t, std::move(addr), port}
    {
    }
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    any_address() noexcept = default;

    address_type type() const noexcept { return impl_.type; }

    string_view hostname() const noexcept
    {
        BOOST_ASSERT(type() == address_type::tcp_address);
        return impl_.address;
    }

    unsigned short port() const noexcept
    {
        BOOST_ASSERT(type() == address_type::tcp_address);
        return impl_.port;
    }

    string_view unix_path() const noexcept
    {
        BOOST_ASSERT(type() == address_type::unix_path);
        return impl_.address;
    }

    void set_host_and_port(std::string hostname, unsigned short port = 3306)
    {
        impl_.type = address_type::tcp_address;
        impl_.address = std::move(hostname);
        impl_.port = port;
    }

    void set_unix_path(std::string path)
    {
        impl_.type = address_type::unix_path;
        impl_.address = std::move(path);
    }

    static any_address make_tcp(std::string hostname, unsigned short port = 3306)
    {
        return any_address(address_type::tcp_address, std::move(hostname), port);
    }

    static any_address make_unix(std::string path)
    {
        return any_address(address_type::unix_path, std::move(path), 0);
    }
};

}  // namespace mysql
}  // namespace boost

#endif
