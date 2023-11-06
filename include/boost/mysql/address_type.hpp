//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ADDRESS_TYPE_HPP
#define BOOST_MYSQL_ADDRESS_TYPE_HPP

namespace boost {
namespace mysql {

enum class address_type
{
    tcp_address,
    unix_path
};

}  // namespace mysql
}  // namespace boost

#endif
