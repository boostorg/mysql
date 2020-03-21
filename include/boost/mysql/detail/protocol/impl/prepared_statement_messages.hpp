#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_PREPARED_STATEMENT_MESSAGES_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_PREPARED_STATEMENT_MESSAGES_HPP_

#include "boost/mysql/detail/protocol/null_bitmap_traits.hpp"

namespace boost {
namespace mysql {
namespace detail {

// Maps from an actual value to a protocol_field_type. Only value's type is used
inline protocol_field_type get_protocol_field_type(
	const value& input
) noexcept
{
	struct visitor
	{
		constexpr auto operator()(std::int32_t) const noexcept { return protocol_field_type::long_; }
		constexpr auto operator()(std::uint32_t) const noexcept { return protocol_field_type::long_; }
		constexpr auto operator()(std::int64_t) const noexcept { return protocol_field_type::longlong; }
		constexpr auto operator()(std::uint64_t) const noexcept { return protocol_field_type::longlong; }
		constexpr auto operator()(std::string_view) const noexcept { return protocol_field_type::varchar; }
		constexpr auto operator()(float) const noexcept { return protocol_field_type::float_; }
		constexpr auto operator()(double) const noexcept { return protocol_field_type::double_; }
		constexpr auto operator()(date) const noexcept { return protocol_field_type::date; }
		constexpr auto operator()(datetime) const noexcept { return protocol_field_type::datetime; }
		constexpr auto operator()(time) const noexcept { return protocol_field_type::time; }
		constexpr auto operator()(std::nullptr_t) const noexcept { return protocol_field_type::null; }
	};
	return std::visit(visitor(), input);
}

// Whether to include the unsigned flag in the statement execute message
// for a given value or not. Only value's type is used
inline bool is_unsigned(
	const value& input
) noexcept
{
	// By default, return false; just for integer types explicitly unsigned return true
	return std::visit([](auto v) {
		using type = decltype(v);
		return std::is_same_v<type, std::uint32_t> ||
			   std::is_same_v<type, std::uint64_t>;
	}, input);
}

// Performs a mapping from T to a type that can be serialized
template <typename T>
struct get_serializable_type { using type = T; };

template <typename T>
using get_serializable_type_t = typename get_serializable_type<T>::type;

template <> struct get_serializable_type<std::uint32_t> { using type = int4; };
template <> struct get_serializable_type<std::int32_t> { using type = int4_signed; };
template <> struct get_serializable_type<std::uint64_t> { using type = int8; };
template <> struct get_serializable_type<std::int64_t> { using type = int8_signed; };
template <> struct get_serializable_type<std::string_view> { using type = string_lenenc; };
template <> struct get_serializable_type<std::nullptr_t> { using type = dummy_serializable; };

template <typename T>
inline get_serializable_type_t<T> to_serializable_type(T input) noexcept
{
	return get_serializable_type_t<T>(input);
}

inline std::size_t get_binary_value_size(
	const value& input,
	const serialization_context& ctx
) noexcept
{
	return std::visit([&ctx](const auto& v) {
		return get_size(to_serializable_type(v), ctx);
	}, input);
}

inline void serialize_binary_value(
	const value& input,
	serialization_context& ctx
) noexcept
{
	std::visit([&ctx](const auto& v) {
		serialize(to_serializable_type(v), ctx);
	}, input);
}

} // detail
} // mysql
} // boost

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::com_stmt_prepare_ok_packet,
	boost::mysql::detail::struct_tag
>::deserialize_(
	com_stmt_prepare_ok_packet& output,
	deserialization_context& ctx
) noexcept
{
	int1 reserved;
	return deserialize_fields(
		ctx,
		output.statement_id,
		output.num_columns,
		output.num_params,
		reserved,
		output.warning_count
	);
}

template <typename ForwardIterator>
inline std::size_t
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::com_stmt_execute_packet<ForwardIterator>,
	boost::mysql::detail::struct_tag
>::get_size_(
	const com_stmt_execute_packet<ForwardIterator>& value,
	const serialization_context& ctx
) noexcept
{
	std::size_t res = 1 + // command ID
		get_size(value.statement_id, ctx) +
		get_size(value.flags, ctx) +
		get_size(value.iteration_count, ctx);
	auto num_params = std::distance(value.params_begin, value.params_end);
	assert(num_params >= 0 && num_params <= 255);
	res += null_bitmap_traits(stmt_execute_null_bitmap_offset, num_params).byte_count();
	res += get_size(value.new_params_bind_flag, ctx);
	res += get_size(com_stmt_execute_param_meta_packet{}, ctx) * num_params;
	for (auto it = value.params_begin; it != value.params_end; ++it)
	{
		res += get_binary_value_size(*it, ctx);
	}
	return res;
}

template <typename ForwardIterator>
inline void
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::com_stmt_execute_packet<ForwardIterator>,
	boost::mysql::detail::struct_tag
>::serialize_(
	const com_stmt_execute_packet<ForwardIterator>& input,
	serialization_context& ctx
) noexcept
{
	serialize(int1(com_stmt_execute_packet<ForwardIterator>::command_id), ctx);
	serialize(input.statement_id, ctx);
	serialize(input.flags, ctx);
	serialize(input.iteration_count, ctx);

	// Number of parameters
	auto num_params = std::distance(input.params_begin, input.params_end);
	assert(num_params >= 0 && num_params <= 255);

	// NULL bitmap (already size zero if num_params == 0)
	null_bitmap_traits traits (stmt_execute_null_bitmap_offset, num_params);
	std::size_t i = 0;
	std::memset(ctx.first(), 0, traits.byte_count()); // Initialize to zeroes
	for (auto it = input.params_begin; it != input.params_end; ++it, ++i)
	{
		if (std::holds_alternative<std::nullptr_t>(*it))
		{
			traits.set_null(ctx.first(), i);
		}
	}
	ctx.advance(traits.byte_count());

	// new parameters bind flag
	serialize(input.new_params_bind_flag, ctx);

	// value metadata
	com_stmt_execute_param_meta_packet meta;
	for (auto it = input.params_begin; it != input.params_end; ++it)
	{
		meta.type = get_protocol_field_type(*it);
		meta.unsigned_flag.value = is_unsigned(*it) ? 0x80 : 0;
		serialize(meta, ctx);
	}

	// actual values
	for (auto it = input.params_begin; it != input.params_end; ++it)
	{
		serialize_binary_value(*it, ctx);
	}
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_PREPARED_STATEMENT_MESSAGES_HPP_ */
