//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_STABLE_CONNECT_PARAMS_IPP
#define BOOST_MYSQL_IMPL_STABLE_CONNECT_PARAMS_IPP

#pragma once

#include <boost/mysql/detail/stable_connect_params.hpp>

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
    auto& impl = access::get_impl(input);

    // Calculate required space
    std::size_t required_size = impl.address.size() + impl.username.size() + impl.password.size() +
                                impl.database.size();

    // Allocate space for strings
    auto ptr = std::make_unique<char[]>(required_size);

    // Copy them to the new buffer
    char* it = ptr.get();
    auto address = copy_string(impl.adjusted_address(), it);
    auto username = copy_string(impl.username, it);
    auto password = copy_string(impl.password, it);
    auto database = copy_string(impl.database, it);

    return {
        any_address(impl.addr_type, address, impl.port),
        handshake_params(
            username,
            password,
            database,
            impl.connection_collation,
            impl.adjusted_ssl_mode(),
            impl.multi_queries
        ),
        std::move(ptr),
    };
}

#endif
