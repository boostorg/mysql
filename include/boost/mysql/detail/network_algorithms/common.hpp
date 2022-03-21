//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_COMMON_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_COMMON_HPP

#include <boost/mysql/error.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/detail/protocol/channel.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>

namespace boost {
namespace mysql {
namespace detail {

using deserialize_row_fn = error_code (*)(
    deserialization_context&,
    const std::vector<field_metadata>&,
    std::vector<value>&
);

} // detail
} // mysql
} // boost


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_COMMON_HPP_ */
