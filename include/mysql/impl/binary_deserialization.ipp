#ifndef INCLUDE_MYSQL_IMPL_BINARY_DESERIALIZATION_IPP_
#define INCLUDE_MYSQL_IMPL_BINARY_DESERIALIZATION_IPP_

#include <variant>
#include "mysql/impl/null_bitmap_traits.hpp"
#include "mysql/impl/tmp.hpp"

namespace mysql
{
namespace detail
{

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
	value_holder<float>,
	value_holder<double>,
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
    	return value_holder<float>();
    case protocol_field_type::double_:
    	return value_holder<double>();
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

inline Error deserialize_binary_row_impl(
	DeserializationContext& ctx,
	const std::vector<field_metadata>& meta,
	std::vector<value>& output
)
{
	// Packet header is already read
	// Number of fields
	auto num_fields = meta.size();
	output.resize(num_fields);

	// Null bitmap
	null_bitmap_traits null_bitmap (binary_row_null_bitmap_offset, num_fields);
	const std::uint8_t* null_bitmap_begin = ctx.first();
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
			auto err = deserialize(output[i], ctx);
			if (err != Error::ok) return err;
		}
	}

	// Check for remaining bytes
	return ctx.empty() ? Error::extra_bytes : Error::ok;
}

}
}

inline mysql::Error mysql::detail::deserialize_binary_value(
	DeserializationContext& ctx,
	const field_metadata& meta,
	value& output
)
{
	auto protocol_value = get_deserializable_type(meta);
	return std::visit([&output, &ctx](auto typed_protocol_value) {
		using type = decltype(typed_protocol_value);
		auto err = deserialize(typed_protocol_value, ctx);
		if (err == Error::ok)
		{
			if constexpr (std::is_constructible_v<value, type>) // not a value holder
			{
				output = typed_protocol_value;
			}
			else if constexpr (is_one_of_v<type, int1, int2>)
			{
				// regular promotion would make this int32_t. Force it be uint32_t
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

inline mysql::error_code mysql::detail::deserialize_binary_row(
	DeserializationContext& ctx,
	const std::vector<field_metadata>& meta,
	std::vector<value>& output
)
{
	return make_error_code(deserialize_binary_row_impl(ctx, meta, output));
}



#endif /* INCLUDE_MYSQL_IMPL_BINARY_DESERIALIZATION_IPP_ */
