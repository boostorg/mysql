#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_BINARY_DESERIALIZATION_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_BINARY_DESERIALIZATION_HPP_

#include "boost/mysql/detail/protocol/serialization.hpp"
#include "boost/mysql/error.hpp"
#include "boost/mysql/value.hpp"
#include "boost/mysql/metadata.hpp"
#include <vector>

namespace mysql
{
namespace detail
{

inline Error deserialize_binary_value(
	DeserializationContext& ctx,
	const field_metadata& meta,
	value& output
);

inline error_code deserialize_binary_row(
	DeserializationContext& ctx,
	const std::vector<field_metadata>& meta,
	std::vector<value>& output
);

}
}

#include "boost/mysql/detail/protocol/impl/binary_deserialization.ipp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_BINARY_DESERIALIZATION_HPP_ */
