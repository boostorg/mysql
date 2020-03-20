#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_QUERY_MESSAGES_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_QUERY_MESSAGES_HPP_

#include "boost/mysql/detail/protocol/protocol_types.hpp"
#include "boost/mysql/detail/aux/get_struct_fields.hpp"
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
