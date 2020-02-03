#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_

#include "mysql/impl/network_algorithms/common.hpp"
#include "mysql/error.hpp"
#include <string_view>

namespace mysql
{
namespace detail
{

template <typename ChannelType>
void execute_query(
	ChannelType& channel,
	std::string_view query,
	channel_resultset_type<ChannelType>& output,
	error_code& err,
	error_info& info
);

template <typename ChannelType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, error_info, channel_resultset_type<ChannelType>))
async_execute_query(
	ChannelType& channel,
	std::string_view query,
	CompletionToken&& token
);

}
}

#include "mysql/impl/network_algorithms/execute_query.ipp"

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_ */
