#ifndef INCLUDE_MYSQL_IMPL_QUERY_HPP_
#define INCLUDE_MYSQL_IMPL_QUERY_HPP_

#include "mysql/resultset.hpp"
#include "mysql/impl/capabilities.hpp"
#include <string_view>

namespace mysql
{
namespace detail
{

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
	resultset<ChannelType, Allocator>& output,
	error_code& err
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
