#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_

#include "mysql/impl/network_algorithms/common.hpp"
#include "mysql/resultset.hpp"
#include <string_view>

namespace mysql
{
namespace detail
{

template <typename StreamType>
void execute_query(
	channel<StreamType>& channel,
	std::string_view query,
	resultset<StreamType>& output,
	error_code& err,
	error_info& info
);

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, error_info, resultset<StreamType>))
async_execute_query(
	channel<StreamType>& chan,
	std::string_view query,
	CompletionToken&& token
);

}
}

#include "mysql/impl/network_algorithms/execute_query.ipp"

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_ */
