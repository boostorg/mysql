#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP_

#include "mysql/impl/network_algorithms/common.hpp"
#include "mysql/resultset.hpp"
#include "mysql/value.hpp"

namespace mysql
{
namespace detail
{

template <typename StreamType, typename ForwardIterator>
void execute_statement(
	channel<StreamType>& channel,
	std::uint32_t statement_id,
	ForwardIterator params_begin,
	ForwardIterator params_end,
	resultset<StreamType>& output,
	error_code& err,
	error_info& info
);

}
}

#include "mysql/impl/network_algorithms/execute_statement.ipp"

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP_ */
