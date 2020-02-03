#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_

#include "mysql/impl/network_algorithms/common.hpp"
#include "mysql/resultset.hpp"
#include <string_view>

namespace mysql
{
namespace detail
{

template <typename StreamType, typename Serializable>
void execute_generic(
	channel<StreamType>& channel,
	const Serializable& request,
	resultset<StreamType>& output,
	error_code& err,
	error_info& info
);

template <typename StreamType, typename Serializable, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, error_info, resultset<StreamType>))
async_execute_generic(
	channel<StreamType>& chan,
	const Serializable& request,
	CompletionToken&& token
);

}
}

#include "mysql/impl/network_algorithms/execute_generic.ipp"

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_ */
