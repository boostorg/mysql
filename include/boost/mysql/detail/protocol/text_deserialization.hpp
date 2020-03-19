#ifndef MYSQL_ASIO_IMPL_DESERIALIZE_ROW_HPP
#define MYSQL_ASIO_IMPL_DESERIALIZE_ROW_HPP

#include "boost/mysql/detail/protocol/serialization.hpp"
#include "boost/mysql/error.hpp"
#include "boost/mysql/value.hpp"
#include "boost/mysql/metadata.hpp"
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

inline Error deserialize_text_value(
	std::string_view from,
	const field_metadata& meta,
	value& output
);

inline error_code deserialize_text_row(
	DeserializationContext& ctx,
	const std::vector<field_metadata>& meta,
	std::vector<value>& output
);

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/text_deserialization.ipp"

#endif
