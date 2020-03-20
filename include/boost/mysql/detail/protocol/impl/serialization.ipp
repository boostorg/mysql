#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_IPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_IPP_

inline boost::mysql::errc boost::mysql::detail::deserialize(
	int_lenenc& output,
	deserialization_context& ctx
) noexcept
{
	int1 first_byte;
	errc err = deserialize(first_byte, ctx);
	if (err != errc::ok)
	{
		return err;
	}

	if (first_byte.value == 0xFC)
	{
		int2 value;
		err = deserialize(value, ctx);
		output.value = value.value;
	}
	else if (first_byte.value == 0xFD)
	{
		int3 value;
		err = deserialize(value, ctx);
		output.value = value.value;
	}
	else if (first_byte.value == 0xFE)
	{
		int8 value;
		err = deserialize(value, ctx);
		output.value = value.value;
	}
	else
	{
		err = errc::ok;
		output.value = first_byte.value;
	}
	return err;
}

inline void boost::mysql::detail::serialize(
	int_lenenc input,
	serialization_context& ctx
) noexcept
{
	if (input.value < 251)
	{
		serialize(int1(static_cast<std::uint8_t>(input.value)), ctx);
	}
	else if (input.value < 0x10000)
	{
		ctx.write(0xfc);
		serialize(int2(static_cast<std::uint16_t>(input.value)), ctx);
	}
	else if (input.value < 0x1000000)
	{
		ctx.write(0xfd);
		serialize(int3(static_cast<std::uint32_t>(input.value)), ctx);
	}
	else
	{
		ctx.write(0xfe);
		serialize(int8(static_cast<std::uint64_t>(input.value)), ctx);
	}
}

inline std::size_t boost::mysql::detail::get_size(
	int_lenenc input, const
	serialization_context&
) noexcept
{
	if (input.value < 251) return 1;
	else if (input.value < 0x10000) return 3;
	else if (input.value < 0x1000000) return 4;
	else return 9;
}

inline boost::mysql::errc boost::mysql::detail::deserialize(
	string_null& output,
	deserialization_context& ctx
) noexcept
{
	auto string_end = std::find(ctx.first(), ctx.last(), 0);
	if (string_end == ctx.last())
	{
		return errc::incomplete_message;
	}
	output.value = get_string(ctx.first(), string_end-ctx.first());
	ctx.set_first(string_end + 1); // skip the null terminator
	return errc::ok;
}

inline boost::mysql::errc boost::mysql::detail::deserialize(
	string_eof& output,
	deserialization_context& ctx
) noexcept
{
	output.value = get_string(ctx.first(), ctx.last()-ctx.first());
	ctx.set_first(ctx.last());
	return errc::ok;
}

inline boost::mysql::errc boost::mysql::detail::deserialize(
	string_lenenc& output,
	deserialization_context& ctx
) noexcept
{
	int_lenenc length;
	errc err = deserialize(length, ctx);
	if (err != errc::ok)
	{
		return err;
	}
	if (!ctx.enough_size(length.value))
	{
		return errc::incomplete_message;
	}

	output.value = get_string(ctx.first(), length.value);
	ctx.advance(length.value);
	return errc::ok;
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_IPP_ */
