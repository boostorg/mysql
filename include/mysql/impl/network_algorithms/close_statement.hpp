#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP_

#include "mysql/impl/network_algorithms/common.hpp"

namespace mysql
{
namespace detail
{

template <typename StreamType>
void close_statement(
	channel<StreamType>& chan,
	std::uint32_t statement_id,
	error_code& errc,
	error_info& info
);

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, error_info))
async_close_statement(
	channel<StreamType>& chan,
	std::uint32_t statement_id,
	CompletionToken&& token
);

}
}

#include "mysql/impl/network_algorithms/close_statement.ipp"

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP_ */
