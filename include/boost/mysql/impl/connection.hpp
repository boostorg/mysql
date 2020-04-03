#ifndef MYSQL_ASIO_IMPL_CONNECTION_IPP
#define MYSQL_ASIO_IMPL_CONNECTION_IPP

#include "boost/mysql/detail/network_algorithms/handshake.hpp"
#include "boost/mysql/detail/network_algorithms/execute_query.hpp"
#include "boost/mysql/detail/network_algorithms/prepare_statement.hpp"
#include <boost/asio/buffer.hpp>

namespace boost {
namespace mysql {
namespace detail {

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

} // detail
} // mysql
} // boost

template <typename Stream>
void boost::mysql::connection<Stream>::handshake(
	const connection_params& params,
	error_code& code,
	error_info& info
)
{
	code.clear();
	info.clear();
	detail::hanshake(
		channel_,
		detail::to_handshake_params(params),
		code,
		info
	);
	// TODO: should we close() the stream in case of error?
}

template <typename Stream>
void boost::mysql::connection<Stream>::handshake(
	const connection_params& params
)
{
	error_code code;
	error_info info;
	handshake(params, code, info);
	detail::check_error_code(code, info);
}

template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	typename boost::mysql::connection<Stream>::handshake_signature
)
boost::mysql::connection<Stream>::async_handshake(
	const connection_params& params,
	CompletionToken&& token,
	error_info* info
)
{
	detail::conditional_clear(info);
	return detail::async_handshake(
		channel_,
		detail::to_handshake_params(params),
		std::forward<CompletionToken>(token),
		info
	);
}

// Query
template <typename Stream>
boost::mysql::resultset<Stream> boost::mysql::connection<Stream>::query(
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
boost::mysql::resultset<Stream> boost::mysql::connection<Stream>::query(
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
	typename boost::mysql::connection<Stream>::query_signature
)
boost::mysql::connection<Stream>::async_query(
	std::string_view query_string,
	CompletionToken&& token,
	error_info* info
)
{
	detail::conditional_clear(info);
	return detail::async_execute_query(
		channel_,
		query_string,
		std::forward<CompletionToken>(token),
		info
	);
}

template <typename Stream>
boost::mysql::prepared_statement<Stream> boost::mysql::connection<Stream>::prepare_statement(
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
boost::mysql::prepared_statement<Stream> boost::mysql::connection<Stream>::prepare_statement(
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

template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	typename boost::mysql::connection<Stream>::prepare_statement_signature
)
boost::mysql::connection<Stream>::async_prepare_statement(
	std::string_view statement,
	CompletionToken&& token,
	error_info* info
)
{
	detail::conditional_clear(info);
	return detail::async_prepare_statement(
		channel_,
		statement,
		std::forward<CompletionToken>(token),
		info
	);
}


#endif
