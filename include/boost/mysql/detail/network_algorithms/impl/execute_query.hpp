#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_

#include "boost/mysql/detail/network_algorithms/execute_generic.hpp"
#include "boost/mysql/detail/protocol/text_deserialization.hpp"

template <typename StreamType>
void mysql::detail::execute_query(
	channel<StreamType>& channel,
	std::string_view query,
	resultset<StreamType>& output,
	error_code& err,
	error_info& info
)
{
	com_query_packet request { string_eof(query) };
	execute_generic(&deserialize_text_row, channel, request, output, err, info);
}


template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	void(mysql::error_code, mysql::error_info, mysql::resultset<StreamType>)
)
mysql::detail::async_execute_query(
	channel<StreamType>& chan,
	std::string_view query,
	CompletionToken&& token
)
{
	com_query_packet request { string_eof(query) };
	return async_execute_generic(&deserialize_text_row, chan, request, std::forward<CompletionToken>(token));
}

#include <boost/asio/unyield.hpp>


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
