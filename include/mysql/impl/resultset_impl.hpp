#ifndef INCLUDE_MYSQL_IMPL_RESULTSET_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_RESULTSET_IMPL_HPP_

#include "mysql/impl/query.hpp"
#include <cassert>

template <typename ChannelType>
const mysql::row* mysql::resultset<ChannelType>::fetch_one(
	error_code& err
)
{
	assert(valid());
	if (complete())
	{
		err.clear();
		return nullptr;
	}
	auto result = detail::fetch_text_row(
		*channel_,
		meta_.fields(),
		buffer_,
		current_row_.values(),
		ok_packet_,
		err
	);
	eof_received_ = result == detail::fetch_result::eof;
	return result == detail::fetch_result::row ? &current_row_ : nullptr;
}



#endif /* INCLUDE_MYSQL_IMPL_RESULTSET_IMPL_HPP_ */
