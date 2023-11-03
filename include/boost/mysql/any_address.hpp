//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ANY_ADDRESS_HPP
#define BOOST_MYSQL_ANY_ADDRESS_HPP

#include <boost/mysql/string_view.hpp>

#include <string>

namespace boost {
namespace mysql {

enum class address_type
{
    tcp,
    unix
};

class any_address_view
{
    address_type type_;
    string_view address_;
    unsigned short port_{};

#ifndef BOOST_MYSQL_DOXYGEN
    friend class any_address;
#endif

    any_address_view(address_type t, string_view addr, unsigned short port) noexcept
        : type_(t), address_(addr), port_(port)
    {
    }

public:
    static any_address_view tcp(string_view hostname, unsigned short port) noexcept
    {
        return any_address_view(address_type::tcp, hostname, port);
    }
    static any_address_view unix(string_view path) noexcept
    {
        return any_address_view(address_type::unix, path, 0);
    }

    address_type type() const noexcept { return type_; }
    string_view hostname() const noexcept
    {
        BOOST_ASSERT(type_ == address_type::tcp);
        return address_;
    }
    unsigned short port() const noexcept
    {
        BOOST_ASSERT(type_ == address_type::tcp);
        return port_;
    }
    string_view unix_path() const noexcept
    {
        BOOST_ASSERT(type_ == address_type::unix);
        return address_;
    }
};

class any_address
{
    address_type type_;
    std::string address_;
    unsigned short port_{};

    any_address(address_type t, std::string&& addr, unsigned short port) noexcept
        : type_(t), address_(std::move(addr)), port_(port)
    {
    }

public:
    explicit any_address(any_address_view view)
        : type_(view.type_), address_(view.address_), port_(view.port_)
    {
    }

    static any_address_view tcp(std::string hostname, unsigned short port) noexcept
    {
        return any_address_view(address_type::tcp, std::move(hostname), port);
    }
    static any_address_view unix(std::string path) noexcept
    {
        return any_address_view(address_type::unix, std::move(path), 0);
    }

    address_type type() const noexcept { return type_; }
    string_view hostname() const noexcept
    {
        BOOST_ASSERT(type_ == address_type::tcp);
        return address_;
    }
    unsigned short port() const noexcept
    {
        BOOST_ASSERT(type_ == address_type::tcp);
        return port_;
    }
    string_view unix_path() const noexcept
    {
        BOOST_ASSERT(type_ == address_type::unix);
        return address_;
    }

    operator any_address_view() const noexcept { return any_address_view(type_, address_, port_); }
};

}  // namespace mysql
}  // namespace boost

#endif
