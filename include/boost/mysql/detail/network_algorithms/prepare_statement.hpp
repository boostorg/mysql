#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_

#include "boost/mysql/detail/network_algorithms/common.hpp"
#include "boost/mysql/prepared_statement.hpp"

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType>
void prepare_statement(
	::boost::mysql::detail::channel<StreamType>& channel,
	std::string_view statement,
	error_code& err,
	error_info& info,
	prepared_statement<StreamType>& output
);

template <typename StreamType>
using prepare_statement_signature = void(error_code, async_handler_arg<prepared_statement<StreamType>>);

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, prepare_statement_signature<StreamType>)
async_prepare_statement(
	channel<StreamType>& channel,
	std::string_view statement,
	CompletionToken&& token
);

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/network_algorithms/impl/prepare_statement.hpp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_ */
