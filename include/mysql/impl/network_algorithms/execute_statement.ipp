#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_IPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_IPP_

#include "mysql/impl/network_algorithms/execute_generic.hpp"
#include "mysql/impl/binary_deserialization.hpp"

template <typename StreamType>
void mysql::detail::execute_statement(
	channel<StreamType>& chan,
	std::uint32_t statement_id,
	const value* params_begin,
	const value* params_end,
	resultset<StreamType>& output,
	error_code& err,
	error_info& info
)
{
	// Compose statement execute message
	com_stmt_execute_packet request {
		int4(statement_id),
		int1(0), // flags
		int4(1), // iteration count
		int1(1), // new params flag: set
		params_begin,
		params_end
	};
	execute_generic(&deserialize_binary_row, chan, request, output, err, info);
}





#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_IPP_ */
