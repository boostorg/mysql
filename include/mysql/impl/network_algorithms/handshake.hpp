#ifndef MYSQL_ASIO_IMPL_HANDSHAKE_HPP
#define MYSQL_ASIO_IMPL_HANDSHAKE_HPP

#include <string_view>
#include "mysql/impl/channel.hpp"
#include "mysql/impl/basic_types.hpp"
#include "mysql/collation.hpp"

namespace mysql
{
namespace detail
{

struct handshake_params
{
	collation connection_collation;
	std::string_view username;
	std::string_view password;
	std::string_view database;
};

template <typename StreamType>
void hanshake(
	channel<StreamType>& channel,
	const handshake_params& params,
	error_code& err,
	error_info& info
);

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, error_info))
async_handshake(
	channel<StreamType>& channel,
	const handshake_params& params,
	CompletionToken&& token
);

}
}

#include "mysql/impl/network_algorithms/handshake.ipp"


#endif
