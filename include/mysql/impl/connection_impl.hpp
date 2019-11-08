#ifndef INCLUDE_MYSQL_IMPL_CONNECTION_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_CONNECTION_IMPL_HPP_

#include "mysql/impl/handshake.hpp"
#include "mysql/impl/query.hpp"
#include <boost/asio/buffer.hpp>

// Handshake
template <typename Stream, typename Allocator>
void mysql::connection<Stream, Allocator>::handshake(
	const connection_params& params,
	error_code& errc
)
{
	detail::hanshake(channel_, params, buffer_, errc);
	// TODO: should we close() the stream in case of error?
}

template <typename Stream, typename Allocator>
void mysql::connection<Stream, Allocator>::handshake(
	const connection_params& params
)
{
	error_code errc;
	handshake(params, errc);
	detail::check_error_code(errc);
}

template <typename Stream, typename Allocator>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code))
mysql::connection<Stream, Allocator>::async_handshake(
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
template <typename Stream, typename Allocator>
mysql::resultset<typename mysql::connection<Stream, Allocator>::channel_type, Allocator>
mysql::connection<Stream, Allocator>::query(
	std::string_view query_string,
	error_code& err
)
{
	resultset<channel_type, Allocator> res;
	detail::execute_query(channel_, query_string, res, err);
	return res;
}

template <typename Stream, typename Allocator>
mysql::resultset<typename mysql::connection<Stream, Allocator>::channel_type, Allocator>
mysql::connection<Stream, Allocator>::query(
	std::string_view query_string
)
{
	resultset<channel_type, Allocator> res;
	error_code err;
	detail::execute_query(channel_, query_string, res, err);
	detail::check_error_code(err);
	return res;
}


#endif /* INCLUDE_MYSQL_IMPL_CONNECTION_IMPL_HPP_ */
