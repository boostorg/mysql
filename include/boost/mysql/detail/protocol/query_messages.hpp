//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_QUERY_MESSAGES_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_QUERY_MESSAGES_HPP

#include "boost/mysql/detail/protocol/serialization.hpp"
#include <tuple>

namespace boost {
namespace mysql {
namespace detail {

struct com_query_packet
{
	string_eof query;

	static constexpr std::uint8_t command_id = 3;
};

template <>
struct get_struct_fields<com_query_packet>
{
	static constexpr auto value = std::make_tuple(
		&com_query_packet::query
	);
};

}
}
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_QUERY_MESSAGES_HPP_ */
