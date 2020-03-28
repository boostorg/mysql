#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_COMMON_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_COMMON_HPP_

#include <boost/asio/coroutine.hpp>
#include <boost/beast/core/async_base.hpp>
#include "boost/mysql/error.hpp"
#include "boost/mysql/async_handler_arg.hpp"
#include "boost/mysql/metadata.hpp"
#include "boost/mysql/detail/protocol/channel.hpp"
#include "boost/mysql/detail/protocol/common_messages.hpp"

namespace boost {
namespace mysql {
namespace detail {

using deserialize_row_fn = error_code (*)(
	deserialization_context&,
	const std::vector<field_metadata>&,
	std::vector<value>&
);

using empty_signature = void(error_code, error_info);

template <typename T>
using r_handler_signature = void(error_code, async_handler_arg<T>);

} // detail
} // mysql
} // boost


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_COMMON_HPP_ */
