#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_DESERIALIZATION_IPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_DESERIALIZATION_IPP_

#include <variant>
#include "boost/mysql/detail/protocol/null_bitmap_traits.hpp"
#include "boost/mysql/detail/aux/tmp.hpp"

namespace boost {
namespace mysql {
namespace detail {

using binary_protocol_value = std::variant<
	int1,
	int2,
	int4,
	int8,
	int1_signed,
	int2_signed,
	int4_signed,
	int8_signed,
	string_lenenc,
	float,
	double,
	date,
	datetime,
	time
>;

template <typename SignedType, typename UnsignedType>
binary_protocol_value get_int_deserializable_type(
	const field_metadata& meta
)
{
	return meta.is_unsigned() ? binary_protocol_value(UnsignedType()) :
			                    binary_protocol_value(SignedType());
}

inline binary_protocol_value get_deserializable_type(
	const field_metadata& meta
)
{
	switch (meta.protocol_type())
	{
    case protocol_field_type::tiny:
    	return get_int_deserializable_type<int1_signed, int1>(meta);
    case protocol_field_type::short_:
    case protocol_field_type::year:
    	return get_int_deserializable_type<int2_signed, int2>(meta);
    case protocol_field_type::int24:
    case protocol_field_type::long_:
    	return get_int_deserializable_type<int4_signed, int4>(meta);
    case protocol_field_type::longlong:
    	return get_int_deserializable_type<int8_signed, int8>(meta);
    case protocol_field_type::float_:
    	return float();
    case protocol_field_type::double_:
    	return double();
    case protocol_field_type::timestamp:
    case protocol_field_type::datetime:
    	return datetime();
    case protocol_field_type::date:
    	return date();
    case protocol_field_type::time:
    	return time();
    // True string types
    case protocol_field_type::varchar:
    case protocol_field_type::var_string:
    case protocol_field_type::string:
    case protocol_field_type::tiny_blob:
    case protocol_field_type::medium_blob:
    case protocol_field_type::long_blob:
    case protocol_field_type::blob:
    case protocol_field_type::enum_:
    case protocol_field_type::set:
    // Anything else that we do not know how to interpret, we return as a binary string
    case protocol_field_type::decimal:
    case protocol_field_type::bit:
    case protocol_field_type::newdecimal:
    case protocol_field_type::geometry:
    default:
    	return string_lenenc();
	}
}

} // detail
} // mysql
} // boost

inline boost::mysql::errc boost::mysql::detail::deserialize_binary_value(
	deserialization_context& ctx,
	const field_metadata& meta,
	value& output
)
{
	auto protocol_value = get_deserializable_type(meta);
	return std::visit([&output, &ctx](auto typed_protocol_value) {
		using type = decltype(typed_protocol_value);
		auto err = deserialize(typed_protocol_value, ctx);
		if (err == errc::ok)
		{
			if constexpr (std::is_constructible_v<value, type>) // not a value holder
			{
				output = typed_protocol_value;
			}
			else if constexpr (is_one_of_v<type, int1, int2>)
			{
				// regular promotion would make this int32_t. Force it be uint32_t
				// TODO: check here
				output = std::uint32_t(typed_protocol_value.value);
			}
			else
			{
				output = typed_protocol_value.value;
			}
		}
		return err;
	}, protocol_value);
}

inline boost::mysql::error_code boost::mysql::detail::deserialize_binary_row(
	deserialization_context& ctx,
	const std::vector<field_metadata>& meta,
	std::vector<value>& output
)
{
	// Skip packet header (it is not part of the message in the binary
	// protocol but it is in the text protocol, so we include it for homogeneity)
	// The caller will have checked we have this byte already for us
	assert(ctx.enough_size(1));
	ctx.advance(1);

	// Number of fields
	auto num_fields = meta.size();
	output.resize(num_fields);

	// Null bitmap
	null_bitmap_traits null_bitmap (binary_row_null_bitmap_offset, num_fields);
	const std::uint8_t* null_bitmap_begin = ctx.first();
	if (!ctx.enough_size(null_bitmap.byte_count())) return make_error_code(errc::incomplete_message);
	ctx.advance(null_bitmap.byte_count());

	// Actual values
	for (std::vector<value>::size_type i = 0; i < output.size(); ++i)
	{
		if (null_bitmap.is_null(null_bitmap_begin, i))
		{
			output[i] = nullptr;
		}
		else
		{
			auto err = deserialize_binary_value(ctx, meta[i], output[i]);
			if (err != errc::ok) return make_error_code(err);
		}
	}

	// Check for remaining bytes
	if (!ctx.empty()) return make_error_code(errc::extra_bytes);

	return error_code();
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_BINARY_DESERIALIZATION_IPP_ */
