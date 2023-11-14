//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECT_PARAMS_HELPERS_IPP
#define BOOST_MYSQL_IMPL_CONNECT_PARAMS_HELPERS_IPP

#pragma once

#include <boost/mysql/handshake_params.hpp>

#include <boost/mysql/detail/connect_params_helpers.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline string_view copy_string(const std::string& input, char*& it)
{
    if (!input.empty())
    {
        std::memcpy(it, input.data(), input.size());
        string_view res(it, input.size());
        it += input.size();
        return res;
    }
    return string_view();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

boost::mysql::detail::stable_connect_params boost::mysql::detail::make_stable(const connect_params& input)
{
    const auto& addr_impl = access::get_impl(input.server_address);

    // Calculate required space. TODO: handle the case where required_size == 0
    std::size_t required_size = addr_impl.address.size() + input.username.size() + input.password.size() +
                                input.database.size();

    // Allocate space for strings
    auto ptr = std::make_unique<char[]>(required_size);

    // Copy them to the new buffer
    char* it = ptr.get();
    auto address = copy_string(addr_impl.address, it);
    auto username = copy_string(input.username, it);
    auto password = copy_string(input.password, it);
    auto database = copy_string(input.database, it);

    return {
        any_address_view{addr_impl.type, address, addr_impl.port},
        handshake_params(
            username,
            password,
            database,
            handshake_params::default_collation,
            adjust_ssl_mode(input.ssl, input.server_address.type()),
            input.multi_queries
        ),
        std::move(ptr),
    };
}

#endif
