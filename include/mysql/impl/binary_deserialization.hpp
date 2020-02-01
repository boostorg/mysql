#ifndef INCLUDE_MYSQL_IMPL_BINARY_DESERIALIZATION_HPP_
#define INCLUDE_MYSQL_IMPL_BINARY_DESERIALIZATION_HPP_

#include "mysql/impl/serialization.hpp"
#include "mysql/error.hpp"
#include "mysql/value.hpp"
#include "mysql/metadata.hpp"
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

#include "mysql/impl/binary_deserialization.ipp"

#endif /* INCLUDE_MYSQL_IMPL_BINARY_DESERIALIZATION_HPP_ */
