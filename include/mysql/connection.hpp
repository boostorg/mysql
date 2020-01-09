#ifndef MYSQL_ASIO_CONNECTION_HPP
#define MYSQL_ASIO_CONNECTION_HPP

#include "mysql/impl/channel.hpp"
#include "mysql/impl/handshake.hpp"
#include "mysql/impl/basic_types.hpp"
#include "mysql/error.hpp"
#include "mysql/resultset.hpp"

namespace mysql
{

struct connection_params
{
	std::string_view username;
	std::string_view password;
	std::string_view database;
	collation connection_collation;

	connection_params(std::string_view username, std::string_view password,
			std::string_view db = "", collation connection_col = collation::utf8_general_ci) :
		username(username),
		password(password),
		database(db),
		connection_collation(connection_col)
	{
	}
};

template <typename Stream>
class connection
{
	using channel_type = detail::channel<Stream>;

	Stream next_level_;
	channel_type channel_;
	detail::bytestring buffer_;
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

	resultset<Stream> query(std::string_view query_string, error_code&);
	resultset<Stream> query(std::string_view query_string);

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
	async_query(std::string_view query_string, CompletionToken&& token);
};

}

#include "mysql/impl/connection.ipp"

#endif
