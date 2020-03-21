#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_HPP_

namespace boost {
namespace mysql {
namespace detail {

template <typename T>
constexpr std::size_t get_int_size()
{
	static_assert(is_fixed_size_int<T>());
	return std::is_same<T, int3>::value ? 3 :
		   std::is_same<T, int6>::value ? 6 : sizeof(T);
}

struct is_command_helper
{
    template <typename T>
    static constexpr std::true_type get(decltype(T::command_id)*);

    template <typename T>
    static constexpr std::false_type get(...);
};

template <typename T>
struct is_command : decltype(is_command_helper::get<T>(nullptr))
{
};

template <std::size_t index, typename T>
errc deserialize_struct(
	[[maybe_unused]] T& output,
	[[maybe_unused]] deserialization_context& ctx
) noexcept
{
	constexpr auto fields = get_struct_fields<T>::value;
	if constexpr (index == std::tuple_size<decltype(fields)>::value)
	{
		return errc::ok;
	}
	else
	{
		constexpr auto pmem = std::get<index>(fields);
		errc err = deserialize(output.*pmem, ctx);
		if (err != errc::ok)
		{
			return err;
		}
		else
		{
			return deserialize_struct<index+1>(output, ctx);
		}
	}
}

template <std::size_t index, typename T>
void serialize_struct(
	[[maybe_unused]] const T& value,
	[[maybe_unused]] serialization_context& ctx
) noexcept
{
	constexpr auto fields = get_struct_fields<T>::value;
	if constexpr (index < std::tuple_size<decltype(fields)>::value)
	{
		auto pmem = std::get<index>(fields);
		serialize(value.*pmem, ctx);
		serialize_struct<index+1>(value, ctx);
	}
}

template <std::size_t index, typename T>
std::size_t get_size_struct(
	[[maybe_unused]] const T& input,
	[[maybe_unused]] const serialization_context& ctx
) noexcept
{
	constexpr auto fields = get_struct_fields<T>::value;
	if constexpr (index == std::tuple_size<decltype(fields)>::value)
	{
		return 0;
	}
	else
	{
		constexpr auto pmem = std::get<index>(fields);
		return get_size_struct<index+1>(input, ctx) +
		       get_size(input.*pmem, ctx);
	}
}

} // detail
} // mysql
} // boost

template <typename T>
constexpr bool boost::mysql::detail::is_fixed_size_int()
{
	return
		std::is_integral<get_value_type_t<T>>::value &&
		std::is_base_of<value_holder<get_value_type_t<T>>, T>::value &&
		!std::is_same<T, int_lenenc>::value;
}

template <typename T>
boost::mysql::errc
boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::fixed_size_int_tag
>::deserialize_(
	T& output,
	deserialization_context& ctx
) noexcept
{
	static_assert(std::is_standard_layout_v<decltype(T::value)>);

	constexpr auto size = get_int_size<T>();
	if (!ctx.enough_size(size))
	{
		return errc::incomplete_message;
	}

	memset(&output.value, 0, sizeof(output.value));
	memcpy(&output.value, ctx.first(), size);
	boost::endian::little_to_native_inplace(output);
	ctx.advance(size);

	return errc::ok;
}

template <typename T>
void boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::fixed_size_int_tag
>::serialize_(
	T input,
	serialization_context& ctx
) noexcept
{
	boost::endian::native_to_little_inplace(input);
	ctx.write(&input.value, get_int_size<T>());
}

template <typename T>
constexpr std::size_t boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::fixed_size_int_tag
>::get_size_(T, const serialization_context&) noexcept
{
	return get_int_size<T>();
}

template <std::size_t N>
boost::mysql::errc boost::mysql::detail::serialization_traits<
	boost::mysql::detail::string_fixed<N>,
	boost::mysql::detail::no_serialization_tag
>::deserialize_(
	string_fixed<N>& output,
	deserialization_context& ctx
) noexcept
{
	if (!ctx.enough_size(N))
	{
		return errc::incomplete_message;
	}
	memcpy(&output.value, ctx.first(), N);
	ctx.advance(N);
	return errc::ok;
}

template <typename T>
boost::mysql::errc
boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::enum_tag
>::deserialize_(
	T& output,
	deserialization_context& ctx
) noexcept
{
	serializable_type value;
	errc err = deserialize(value, ctx);
	if (err != errc::ok)
	{
		return err;
	}
	output = static_cast<T>(value.value);
	return errc::ok;
}

template <typename T>
std::size_t boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::enum_tag
>::get_size_(T, const serialization_context&) noexcept
{
	return get_int_size<serializable_type>();
}

template <typename T>
boost::mysql::errc
boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::floating_point_tag
>::deserialize_(T& output, deserialization_context& ctx) noexcept
{
	// Size check
	if (!ctx.enough_size(sizeof(T))) return errc::incomplete_message;

	// Endianness conversion
	// Boost.Endian support for floats start at 1.71. TODO: maybe update requirements and CI
#if BOOST_ENDIAN_BIG_BYTE
	char buf [sizeof(T)];
	std::memcpy(buf, ctx.first(), sizeof(T));
	std::reverse(buf, buf + sizeof(T));
	std::memcpy(&output, buf, sizeof(T));
#else
	std::memcpy(&output, ctx.first(), sizeof(T));
#endif
	ctx.advance(sizeof(T));
	return errc::ok;
}

template <typename T>
void
boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::floating_point_tag
>::serialize_(T input, serialization_context& ctx) noexcept
{
	// Endianness conversion
#if BOOST_ENDIAN_BIG_BYTE
	char buf [sizeof(T)];
	std::memcpy(buf, &input, sizeof(T));
	std::reverse(buf, buf + sizeof(T));
	ctx.write(buf, sizeof(T));
#else
	ctx.write(&input, sizeof(T));
#endif
}

template <typename T>
boost::mysql::errc
boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::struct_tag
>::deserialize_(T& output, deserialization_context& ctx) noexcept
{
	return deserialize_struct<0>(output, ctx);
}

template <typename T>
void
boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::struct_tag
>::serialize_(
	[[maybe_unused]] const T& input,
	[[maybe_unused]] serialization_context& ctx
) noexcept
{
	// For commands, add the command ID. Commands are only sent by the client,
	// so this is not considered in the deserialization functions.
	if constexpr (is_command<T>::value)
	{
		serialize(int1(T::command_id), ctx);
	}
	serialize_struct<0>(input, ctx);
}

template <typename T>
std::size_t
boost::mysql::detail::serialization_traits<
	T,
	boost::mysql::detail::struct_tag
>::get_size_(const T& input, const serialization_context& ctx) noexcept
{
	std::size_t res = is_command<T>::value ? 1 : 0;
	res += get_size_struct<0>(input, ctx);
	return res;
}

template <typename Serializable, typename Allocator>
void boost::mysql::detail::serialize_message(
	const Serializable& input,
	capabilities caps,
	basic_bytestring<Allocator>& buffer
)
{
	serialization_context ctx (caps);
	std::size_t size = get_size(input, ctx);
	buffer.resize(size);
	ctx.set_first(buffer.data());
	serialize(input, ctx);
	assert(ctx.first() == buffer.data() + buffer.size());
}

template <typename Deserializable>
boost::mysql::error_code boost::mysql::detail::deserialize_message(
	Deserializable& output,
	deserialization_context& ctx
)
{
	auto err = deserialize(output, ctx);
	if (err != errc::ok) return make_error_code(err);
	if (!ctx.empty()) return make_error_code(errc::extra_bytes);
	return error_code();
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_HPP_ */
