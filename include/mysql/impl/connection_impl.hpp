#ifndef INCLUDE_MYSQL_IMPL_CONNECTION_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_CONNECTION_IMPL_HPP_

#include "mysql/impl/handshake.hpp"
#include <boost/asio/buffer.hpp>

template <typename Stream>
void mysql::connection<Stream>::handshake(
	const connection_params& params,
	error_code& errc
)
{
	detail::hanshake(channel_, params, buffer_, caps_, errc);
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
		caps_,
		std::forward<CompletionToken>(token)
	);
}



#endif /* INCLUDE_MYSQL_IMPL_CONNECTION_IMPL_HPP_ */
