#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_

#include "boost/mysql/detail/network_algorithms/common.hpp"
#include "boost/mysql/detail/network_algorithms/execute_generic.hpp"
#include "boost/mysql/resultset.hpp"
#include <string_view>

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType>
void execute_query(
	channel<StreamType>& channel,
	std::string_view query,
	resultset<StreamType>& output,
	error_code& err,
	error_info& info
);

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, execute_generic_signature<StreamType>)
async_execute_query(
	channel<StreamType>& chan,
	std::string_view query,
	CompletionToken&& token
);

}
}
}

#include "boost/mysql/detail/network_algorithms/impl/execute_query.hpp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_QUERY_HPP_ */
