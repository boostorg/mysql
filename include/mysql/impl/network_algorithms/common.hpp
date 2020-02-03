#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_COMMON_HPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_COMMON_HPP_

#include "mysql/impl/channel.hpp"
#include "mysql/resultset.hpp"

namespace mysql
{
namespace detail
{

template <typename ChannelType>
using channel_resultset_type = resultset<channel_stream_type<ChannelType>>;

enum class fetch_result
{
	error,
	row,
	eof
};

}
}



#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_COMMON_HPP_ */
