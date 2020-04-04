#ifndef MYSQL_ASIO_IMPL_HANDSHAKE_HPP
#define MYSQL_ASIO_IMPL_HANDSHAKE_HPP

#include <string_view>
#include "boost/mysql/detail/network_algorithms/common.hpp"
#include "boost/mysql/detail/protocol/channel.hpp"
#include "boost/mysql/detail/protocol/protocol_types.hpp"
#include "boost/mysql/collation.hpp"

namespace boost {
namespace mysql {
namespace detail {

struct handshake_params
{
	collation connection_collation;
	std::string_view username;
	std::string_view password;
	std::string_view database;
	bool use_ssl; // TODO: change this to an enum
};

template <typename StreamType>
void hanshake(
	channel<StreamType>& channel,
	const handshake_params& params,
	error_code& err,
	error_info& info
);

using handshake_signature = empty_signature;

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, handshake_signature)
async_handshake(
	channel<StreamType>& channel,
	const handshake_params& params,
	CompletionToken&& token,
	error_info* info
);

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/network_algorithms/impl/handshake.hpp"


#endif
