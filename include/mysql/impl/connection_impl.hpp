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
	// TODO: should we close() the stream?
}



#endif /* INCLUDE_MYSQL_IMPL_CONNECTION_IMPL_HPP_ */
