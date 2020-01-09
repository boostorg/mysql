#ifndef MYSQL_ASIO_IMPL_CONNECTION_IPP
#define MYSQL_ASIO_IMPL_CONNECTION_IPP

#include "mysql/impl/handshake.hpp"
#include "mysql/impl/query.hpp"
#include <boost/asio/buffer.hpp>

// Handshake
template <typename Stream>
void mysql::connection<Stream>::handshake(
	const connection_params& params,
	error_code& errc
)
{
	detail::hanshake(channel_, params, buffer_, errc);
	// TODO: should we close() the stream in case of error?
}

template <typename Stream>
void mysql::connection<Stream>::handshake(
	const connection_params& params
)
{
	error_code errc;
	handshake(params, errc);
	detail::check_error_code(errc);
}

template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code))
mysql::connection<Stream>::async_handshake(
	const connection_params& params,
	CompletionToken&& token
)
{
	return detail::async_handshake(
		channel_,
		params,
		buffer_,
		std::forward<CompletionToken>(token)
	);
}

// Query
template <typename Stream>
mysql::resultset<Stream> mysql::connection<Stream>::query(
	std::string_view query_string,
	error_code& err
)
{
	resultset<Stream> res;
	detail::execute_query(channel_, query_string, res, err);
	return res;
}

template <typename Stream>
mysql::resultset<Stream> mysql::connection<Stream>::query(
	std::string_view query_string
)
{
	resultset<Stream> res;
	error_code err;
	detail::execute_query(channel_, query_string, res, err);
	detail::check_error_code(err);
	return res;
}

template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code, mysql::resultset<Stream>))
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


#endif