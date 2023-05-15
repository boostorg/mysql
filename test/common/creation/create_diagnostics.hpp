//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_DIAGNOSTICS_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_DIAGNOSTICS_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>

namespace boost {
namespace mysql {
namespace test {

inline diagnostics create_server_diag(string_view s)
{
    diagnostics res;
    detail::diagnostics_access::assign_server(res, s);
    return res;
}

inline diagnostics create_client_diag(string_view s)
{
    diagnostics res;
    detail::diagnostics_access::assign_client(res, s);
    return res;
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
