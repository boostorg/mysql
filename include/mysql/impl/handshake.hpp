#ifndef INCLUDE_MYSQL_IMPL_HANDSHAKE_HPP_
#define INCLUDE_MYSQL_IMPL_HANDSHAKE_HPP_

#include <string_view>
#include <boost/asio/async_result.hpp>
#include "mysql/impl/channel.hpp"
#include "mysql/impl/capabilities.hpp"
#include "mysql/impl/constants.hpp"
#include "mysql/impl/basic_types.hpp"

namespace mysql
{
namespace detail
{

struct handshake_params
{
	CharacterSetLowerByte character_set;
	std::string_view username;
	std::string_view password;
	std::string_view database;
};

template <typename ChannelType, typename Allocator>
void hanshake(ChannelType& channel, const handshake_params& params,
		bytestring<Allocator>& buffer, capabilities& output_capabilities, error_code& err);

template <typename ChannelType, typename Allocator, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, capabilities))
async_handshake(ChannelType& channel, const handshake_params& params,
		bytestring<Allocator>& buffer, CompletionToken&& token);

}
}

#include "mysql/impl/handshake_impl.hpp"


#endif /* INCLUDE_MYSQL_IMPL_HANDSHAKE_HPP_ */
