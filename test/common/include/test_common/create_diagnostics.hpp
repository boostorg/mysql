//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_CREATE_DIAGNOSTICS_HPP
#define BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_CREATE_DIAGNOSTICS_HPP

#include <boost/mysql/diagnostics.hpp>

#include <boost/mysql/detail/access.hpp>

#include <string_view>

namespace boost {
namespace mysql {
namespace test {

inline diagnostics create_server_diag(std::string_view s)
{
    diagnostics res;
    detail::access::get_impl(res).assign_server(std::string(s));
    return res;
}

inline diagnostics create_client_diag(std::string_view s)
{
    diagnostics res;
    detail::access::get_impl(res).assign_client(std::string(s));
    return res;
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
