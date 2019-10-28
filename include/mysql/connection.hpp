#ifndef INCLUDE_MYSQL_CONNECTION_HPP_
#define INCLUDE_MYSQL_CONNECTION_HPP_

#include "mysql/impl/channel.hpp"
#include "mysql/impl/handshake.hpp"
#include "mysql/impl/basic_types.hpp"
#include "mysql/error.hpp"

namespace mysql
{

using connection_params = detail::handshake_params; // TODO: do we think this interface is good enough?

template <typename Stream, typename Allocator=std::allocator<std::uint8_t>>
class connection
{
	Stream next_level_;
	detail::channel<Stream> channel_;
	detail::bytestring<Allocator> buffer_;
public:
	template <typename... Args>
	connection(Args&&... args) :
		next_level_(std::forward<Args>(args)...),
		channel_(next_level_)
	{
	}

	Stream& next_level() { return next_level_; }
	const Stream& next_level() const { return next_level_; }

	void handshake(const connection_params& params, error_code& ec);
	void handshake(const connection_params& params);

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code))
	async_handshake(const connection_params& params, CompletionToken&& token);


};

}

#include "mysql/impl/connection_impl.hpp"

#endif /* INCLUDE_MYSQL_CONNECTION_HPP_ */
