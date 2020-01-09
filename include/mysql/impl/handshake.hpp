#ifndef INCLUDE_MYSQL_IMPL_HANDSHAKE_HPP_
#define INCLUDE_MYSQL_IMPL_HANDSHAKE_HPP_

#include <string_view>
#include <boost/asio/async_result.hpp>
#include "mysql/impl/channel.hpp"
#include "mysql/impl/constants.hpp"
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

template <typename ChannelType>
void hanshake(
	ChannelType& channel,
	const handshake_params& params,
	bytestring& buffer,
	error_code& err
);

template <typename ChannelType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code))
async_handshake(
	ChannelType& channel,
	const handshake_params& params,
	bytestring& buffer,
	CompletionToken&& token
);

}
}

#include "mysql/impl/handshake.ipp"


#endif /* INCLUDE_MYSQL_IMPL_HANDSHAKE_HPP_ */
