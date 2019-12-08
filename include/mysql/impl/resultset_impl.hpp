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

template <typename ChannelType>
const mysql::row* mysql::resultset<ChannelType>::fetch_one()
{
	error_code errc;
	const row* res = fetch_one(errc);
	detail::check_error_code(errc);
	return res;
}

template <typename ChannelType>
std::vector<mysql::owning_row> mysql::resultset<ChannelType>::fetch_many(
	std::size_t count,
	error_code& err
)
{
	assert(valid());

	err.clear();
	std::vector<mysql::owning_row> res;

	if (!complete()) // support calling fetch on already exhausted resultset
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			detail::bytestring buff;
			std::vector<value> values;

			auto result = detail::fetch_text_row(
				*channel_,
				meta_.fields(),
				buff,
				values,
				ok_packet_,
				err
			);
			eof_received_ = result == detail::fetch_result::eof;
			if (result == detail::fetch_result::row)
			{
				res.emplace_back(std::move(values), meta_.fields(), std::move(buff));
			}
			else
			{
				break;
			}
		}
	}

	return res;
}

template <typename ChannelType>
std::vector<mysql::owning_row> mysql::resultset<ChannelType>::fetch_many(
	std::size_t count
)
{
	error_code errc;
	auto res = fetch_many(count, errc);
	detail::check_error_code(errc);
	return res;
}



#endif /* INCLUDE_MYSQL_IMPL_RESULTSET_IMPL_HPP_ */
