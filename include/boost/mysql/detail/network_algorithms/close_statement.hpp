#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP_

#include "boost/mysql/detail/network_algorithms/common.hpp"

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType>
void close_statement(
	channel<StreamType>& chan,
	std::uint32_t statement_id,
	error_code& code,
	error_info& info
);

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, error_info))
async_close_statement(
	channel<StreamType>& chan,
	std::uint32_t statement_id,
	CompletionToken&& token
);

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/network_algorithms/impl/close_statement.hpp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP_ */
