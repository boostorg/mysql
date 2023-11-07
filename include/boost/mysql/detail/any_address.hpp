//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ANY_ADDRESS_HPP
#define BOOST_MYSQL_DETAIL_ANY_ADDRESS_HPP

#include <boost/mysql/address_type.hpp>
#include <boost/mysql/string_view.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct any_address
{
    address_type type{address_type::tcp_address};
    string_view address;
    unsigned short port{};
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
