#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_

#include "boost/mysql/detail/network_algorithms/common.hpp"
#include "boost/mysql/resultset.hpp"
#include <string_view>

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType, typename Serializable>
void execute_generic(
	deserialize_row_fn deserializer,
	channel<StreamType>& channel,
	const Serializable& request,
	resultset<StreamType>& output,
	error_code& err,
	error_info& info
);

template <typename StreamType>
using execute_generic_signature = void(error_code, resultset<StreamType>);

template <typename StreamType, typename Serializable, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, execute_generic_signature<StreamType>)
async_execute_generic(
	deserialize_row_fn deserializer,
	channel<StreamType>& chan,
	const Serializable& request,
	CompletionToken&& token,
	error_info* info
);

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/network_algorithms/impl/execute_generic.hpp"

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_ */
