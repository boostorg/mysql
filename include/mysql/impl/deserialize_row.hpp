#ifndef MYSQL_ASIO_IMPL_DESERIALIZE_ROW_HPP
#define MYSQL_ASIO_IMPL_DESERIALIZE_ROW_HPP

#include "mysql/impl/serialization.hpp"
#include "mysql/error.hpp"
#include "mysql/value.hpp"
#include "mysql/metadata.hpp"
#include "mysql/value.hpp"
#include <vector>

namespace mysql
{
namespace detail
{

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

}
}

#include "mysql/impl/deserialize_row.ipp"

#endif