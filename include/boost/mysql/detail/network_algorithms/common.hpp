#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_COMMON_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_COMMON_HPP_

#include <boost/asio/coroutine.hpp>
#include <boost/beast/core/async_base.hpp>
#include "boost/mysql/error.hpp"
#include "boost/mysql/metadata.hpp"
#include "boost/mysql/detail/protocol/channel.hpp"
#include "boost/mysql/detail/protocol/messages.hpp"

namespace mysql
{
namespace detail
{

using deserialize_row_fn = error_code (*)(
	DeserializationContext&,
	const std::vector<field_metadata>&,
	std::vector<value>&
);

}
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_COMMON_HPP_ */
