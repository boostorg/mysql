#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_

#include "mysql/impl/network_algorithms/common.hpp"
#include "mysql/prepared_statement.hpp"

namespace mysql
{
namespace detail
{

template <typename StreamType>
void prepare_statement(
	channel<StreamType>& channel,
	std::string_view statement,
	error_code& err,
	error_info& info,
	prepared_statement<StreamType>& output
);

}
}

#include "mysql/impl/network_algorithms/prepare_statement.ipp"

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_ */
