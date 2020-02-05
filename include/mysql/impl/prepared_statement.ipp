#ifndef INCLUDE_MYSQL_IMPL_PREPARED_STATEMENT_IPP_
#define INCLUDE_MYSQL_IMPL_PREPARED_STATEMENT_IPP_

#include "mysql/impl/network_algorithms/execute_statement.hpp"

template <typename Stream>
template <typename ForwardIterator>
mysql::resultset<Stream> mysql::prepared_statement<Stream>::execute(
	ForwardIterator params_first,
	ForwardIterator params_last,
	error_code& err,
	error_info& info
) const
{
	assert(valid());

	mysql::resultset<Stream> res;
	detail::execute_statement(
		*channel_,
		stmt_msg_.statement_id.value,
		params_first,
		params_last,
		res,
		err,
		info
	);
	return res;
}



#endif /* INCLUDE_MYSQL_IMPL_PREPARED_STATEMENT_IPP_ */
