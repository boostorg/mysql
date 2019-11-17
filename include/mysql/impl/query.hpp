#ifndef INCLUDE_MYSQL_IMPL_QUERY_HPP_
#define INCLUDE_MYSQL_IMPL_QUERY_HPP_

#include "mysql/resultset.hpp"
#include "mysql/impl/capabilities.hpp"
#include <string_view>

namespace mysql
{
namespace detail
{

template <typename ChannelType>
using channel_stream_type = typename ChannelType::stream_type;

template <typename ChannelType, typename Allocator>
using channel_resultset_type = resultset<channel_stream_type<ChannelType>, Allocator>;

enum class fetch_result
{
	error,
	row,
	eof
};

template <typename ChannelType, typename Allocator>
void execute_query(
	ChannelType& channel,
	std::string_view query,
	channel_resultset_type<ChannelType, Allocator>& output,
	error_code& err
);

template <typename ChannelType, typename Allocator, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, channel_resultset_type<ChannelType, Allocator>))
async_execute_query(
	ChannelType& channel,
	std::string_view query,
	CompletionToken&& token
);


template <typename ChannelType, typename Allocator>
fetch_result fetch_text_row(
	ChannelType& channel,
	const std::vector<field_metadata>& meta,
	bytestring<Allocator>& buffer,
	std::vector<value>& output_values,
	msgs::ok_packet& output_ok_packet,
	error_code& err
);

}
}

#include "mysql/impl/query_impl.hpp"

#endif /* INCLUDE_MYSQL_IMPL_QUERY_HPP_ */
