#ifndef MYSQL_ASIO_IMPL_CONNECTION_IPP
#define MYSQL_ASIO_IMPL_CONNECTION_IPP

#include "mysql/impl/network_algorithms/handshake.hpp"
#include "mysql/impl/network_algorithms/execute_query.hpp"
#include "mysql/impl/network_algorithms/prepare_statement.hpp"
#include <boost/asio/buffer.hpp>

namespace mysql
{
namespace detail
{

inline handshake_params to_handshake_params(
	const connection_params& input
)
{
	return detail::handshake_params {
		input.connection_collation,
		input.username,
		input.password,
		input.database
	};
}

}
}

template <typename Stream>
void mysql::connection<Stream>::handshake(
	const connection_params& params,
	error_code& errc,
	error_info& info
)
{
	errc.clear();
	info.clear();
	detail::hanshake(
		channel_,
		detail::to_handshake_params(params),
		buffer_,
		errc,
		info
	);
	// TODO: should we close() the stream in case of error?
}

template <typename Stream>
void mysql::connection<Stream>::handshake(
	const connection_params& params
)
{
	error_code errc;
	error_info info;
	handshake(params, errc, info);
	detail::check_error_code(errc, info);
}

template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code, mysql::error_info))
mysql::connection<Stream>::async_handshake(
	const connection_params& params,
	CompletionToken&& token
)
{
	return detail::async_handshake(
		channel_,
		detail::to_handshake_params(params),
		buffer_,
		std::forward<CompletionToken>(token)
	);
}

// Query
template <typename Stream>
mysql::resultset<Stream> mysql::connection<Stream>::query(
	std::string_view query_string,
	error_code& err,
	error_info& info
)
{
	err.clear();
	info.clear();
	resultset<Stream> res;
	detail::execute_query(channel_, query_string, res, err, info);
	return res;
}

template <typename Stream>
mysql::resultset<Stream> mysql::connection<Stream>::query(
	std::string_view query_string
)
{
	resultset<Stream> res;
	error_code err;
	error_info info;
	detail::execute_query(channel_, query_string, res, err, info);
	detail::check_error_code(err, info);
	return res;
}

template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	void(mysql::error_code, mysql::error_info, mysql::resultset<Stream>)
)
mysql::connection<Stream>::async_query(
	std::string_view query_string,
	CompletionToken&& token
)
{
	return detail::async_execute_query(
		channel_,
		query_string,
		std::forward<CompletionToken>(token)
	);
}

template <typename Stream>
mysql::prepared_statement<Stream> mysql::connection<Stream>::prepare_statement(
	std::string_view statement,
	error_code& err,
	error_info& info
)
{
	mysql::prepared_statement<Stream> res;
	err.clear();
	info.clear();
	detail::prepare_statement(channel_, statement, err, info, res);
	return res;
}

template <typename Stream>
mysql::prepared_statement<Stream> mysql::connection<Stream>::prepare_statement(
	std::string_view statement
)
{
	mysql::prepared_statement<Stream> res;
	error_code err;
	error_info info;
	detail::prepare_statement(channel_, statement, err, info, res);
	detail::check_error_code(err, info);
	return res;
}


#endif
