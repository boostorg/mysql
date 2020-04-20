//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_IPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_IPP

#include <limits>

namespace boost {
namespace mysql {
namespace detail {

inline std::string_view get_string(
	const std::uint8_t* from,
	std::size_t size
)
{
	return std::string_view(reinterpret_cast<const char*>(from), size);
}

inline errc deserialize_binary_date(
	date& output,
	std::uint8_t length,
	deserialization_context& ctx
) noexcept
{
	int2 year;
	int1 month;
	int1 day;

	if (length >= 4) // if length is zero, year, month and day are zero
	{
		auto err = deserialize_fields(ctx, year, month, day);
		if (err != errc::ok) return err;
	}

	// TODO: how does this handle zero dates?
	::date::year_month_day ymd (::date::year(year.value), ::date::month(month.value), ::date::day(day.value));
	output = date(ymd);
	return errc::ok;
}

// Does not add the length prefix byte
inline void serialize_binary_ymd(
	const ::date::year_month_day& ymd,
	serialization_context& ctx
) noexcept
{
	serialize_fields(
		ctx,
		int2(static_cast<std::uint16_t>(static_cast<int>(ymd.year()))),
		int1(static_cast<std::uint8_t>(static_cast<unsigned>(ymd.month()))),
		int1(static_cast<std::uint8_t>(static_cast<unsigned>(ymd.day())))
	);
}

struct broken_datetime
{
	::date::year_month_day ymd;
	::date::time_of_day<std::chrono::microseconds> tod;

	broken_datetime(const datetime& input) noexcept :
		ymd(::date::floor<::date::days>(input)),
		tod(input - ::date::floor<::date::days>(input))
	{
	}

	// Doesn't count the first length byte
	std::uint8_t binary_serialized_length() const noexcept
	{
		std::uint8_t res = 11; // base length
		if (tod.subseconds().count() == 0)
		{
			res -= 4;
			if (tod.seconds().count() == 0 &&
			    tod.minutes().count() == 0 &&
				tod.hours().count() == 0)
			{
				res -= 3;
			}
		}
		return res;
	}
};

struct broken_time
{
	::date::days days;
	std::chrono::hours hours;
	std::chrono::minutes minutes;
	std::chrono::seconds seconds;
	std::chrono::microseconds microseconds;

	broken_time(const time& input) noexcept :
		days(std::chrono::duration_cast<::date::days>(input)),
		hours(std::chrono::duration_cast<std::chrono::hours>(input % ::date::days(1))),
		minutes(std::chrono::duration_cast<std::chrono::minutes>(input % std::chrono::hours(1))),
		seconds(std::chrono::duration_cast<std::chrono::seconds>(input % std::chrono::minutes(1))),
		microseconds(input % std::chrono::seconds(1))
	{
	}

	// Doesn't count the first length byte
	std::uint8_t binary_serialized_length() const noexcept
	{
		std::uint8_t res = 12;
		if (microseconds.count() == 0)
		{
			res -= 4;
			if (seconds.count() == 0 &&
				minutes.count() == 0 &&
				hours.count() == 0 &&
				days.count() == 0)
			{
				res -= 8;
			}
		}
		return res;
	}
};

} // detail
} // mysql
} // boost

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::int_lenenc,
	boost::mysql::detail::serialization_tag::none
>::deserialize_(
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


inline void
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::int_lenenc,
	boost::mysql::detail::serialization_tag::none
>::serialize_(
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

inline std::size_t
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::int_lenenc,
	boost::mysql::detail::serialization_tag::none
>::get_size_(
	int_lenenc input,
	const serialization_context&
) noexcept
{
	if (input.value < 251) return 1;
	else if (input.value < 0x10000) return 3;
	else if (input.value < 0x1000000) return 4;
	else return 9;
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::string_null,
	boost::mysql::detail::serialization_tag::none
>::deserialize_(
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

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::string_eof,
	boost::mysql::detail::serialization_tag::none
>::deserialize_(
	string_eof& output,
	deserialization_context& ctx
) noexcept
{
	output.value = get_string(ctx.first(), ctx.last()-ctx.first());
	ctx.set_first(ctx.last());
	return errc::ok;
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::string_lenenc,
	boost::mysql::detail::serialization_tag::none
>::deserialize_(
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
	if (length.value > std::numeric_limits<std::size_t>::max())
	{
		return errc::protocol_value_error;
	}
	auto len = static_cast<std::size_t>(length.value);
	if (!ctx.enough_size(len))
	{
		return errc::incomplete_message;
	}

	output.value = get_string(ctx.first(), len);
	ctx.advance(len);
	return errc::ok;
}

inline std::size_t
boost::mysql::detail::serialization_traits<
	boost::mysql::date,
	boost::mysql::detail::serialization_tag::none
>::get_size_(
	const date&,
	const serialization_context&
) noexcept
{
	// TODO: consider zero dates?
	return 5; // length, year, month, day
}

inline void
boost::mysql::detail::serialization_traits<
	boost::mysql::date,
	boost::mysql::detail::serialization_tag::none
>::serialize_(
	const date& input,
	serialization_context& ctx
) noexcept
{
	// TODO: consider zero dates?
	serialize(int1(4), ctx); //
	serialize_binary_ymd(::date::year_month_day (input), ctx);
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::date,
	boost::mysql::detail::serialization_tag::none
>::deserialize_(
	date& output,
	deserialization_context& ctx
) noexcept
{
	int1 length;
	auto err = deserialize(length, ctx);
	if (err != errc::ok) return err;
	return deserialize_binary_date(output, length.value, ctx);
}

// datetime
inline std::size_t
boost::mysql::detail::serialization_traits<
	boost::mysql::datetime,
	boost::mysql::detail::serialization_tag::none
>::get_size_(
	const datetime& input,
	const serialization_context&
) noexcept
{
	broken_datetime dt (input);
	return dt.binary_serialized_length() + 1; // extra length prefix byte
}

inline void
boost::mysql::detail::serialization_traits<
	boost::mysql::datetime,
	boost::mysql::detail::serialization_tag::none
>::serialize_(
	const datetime& input,
	serialization_context& ctx
) noexcept
{
	broken_datetime brokendt (input);
	auto length = brokendt.binary_serialized_length();
	serialize(int1(length), ctx);
	if (length >= 4) // TODO: refactor these magic constants
	{
		serialize_binary_ymd(brokendt.ymd, ctx);
	}
	if (length >= 7)
	{
		serialize_fields(
			ctx,
			int1(static_cast<std::uint8_t>(brokendt.tod.hours().count())),
			int1(static_cast<std::uint8_t>(brokendt.tod.minutes().count())),
			int1(static_cast<std::uint8_t>(brokendt.tod.seconds().count()))
		);
	}
	if (length >= 11)
	{
		auto micros = static_cast<std::uint32_t>(brokendt.tod.subseconds().count());
		serialize(int4(micros), ctx);
	}
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::datetime,
	boost::mysql::detail::serialization_tag::none
>::deserialize_(
	datetime& output,
	deserialization_context& ctx
) noexcept
{
	int1 length;
	date date_part;
	int1 hours;
	int1 minutes;
	int1 seconds;
	int4 micros;

	// Deserialize length
	auto err = deserialize(length, ctx);
	if (err != errc::ok) return err;

	// Based on length, deserialize the rest of the fields
	err = deserialize_binary_date(date_part, length.value, ctx);
	if (err != errc::ok) return err;
	if (length.value >= 7)
	{
		err = deserialize_fields(ctx, hours, minutes, seconds);
		if (err != errc::ok) return err;
	}
	if (length.value >= 11)
	{
		err = deserialize(micros, ctx);
		if (err != errc::ok) return err;
	}

	// Compose the final datetime. Doing time of day and date separately to avoid overflow
	auto time_of_day_part = std::chrono::hours(hours.value) + std::chrono::minutes(minutes.value) +
		std::chrono::seconds(seconds.value) + std::chrono::microseconds(micros.value);
	output = date_part + time_of_day_part;
	return errc::ok;
}

// time
inline std::size_t
boost::mysql::detail::serialization_traits<
	boost::mysql::time,
	boost::mysql::detail::serialization_tag::none
>::get_size_(
	const time& input,
	const serialization_context&
) noexcept
{
	return broken_time(input).binary_serialized_length() + 1; // length byte
}

inline void
boost::mysql::detail::serialization_traits<
	boost::mysql::time,
	boost::mysql::detail::serialization_tag::none
>::serialize_(
	const time& input,
	serialization_context& ctx
) noexcept
{
	broken_time broken (input);
	auto length = broken.binary_serialized_length();
	serialize(int1(length), ctx);
	if (length >= 8) // TODO: magic constants
	{
		int1 is_negative (input.count() < 0 ? 1 : 0);
		serialize_fields(
			ctx,
			is_negative,
			int4(static_cast<std::uint32_t>(std::abs(broken.days.count()))),
			int1(static_cast<std::uint8_t>(std::abs(broken.hours.count()))),
			int1(static_cast<std::uint8_t>(std::abs(broken.minutes.count()))),
			int1(static_cast<std::uint8_t>(std::abs(broken.seconds.count())))
		);
	}
	if (length >= 12)
	{
		auto micros = static_cast<std::uint32_t>(std::abs(broken.microseconds.count()));
		serialize(int4(micros), ctx);
	}
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::time,
	boost::mysql::detail::serialization_tag::none
>::deserialize_(
	time& output,
	deserialization_context& ctx
) noexcept
{
	// Length
	int1 length;
	auto err = deserialize(length, ctx);
	if (err != errc::ok) return err;

	int1 is_negative (0);
	int4 days (0);
	int1 hours (0);
	int1 minutes(0);
	int1 seconds(0);
	int4 microseconds(0);

	if (length.value >= 8)
	{
		err = deserialize_fields(
			ctx,
			is_negative,
			days,
			hours,
			minutes,
			seconds
		);
		if (err != errc::ok) return err;
	}
	if (length.value >= 12)
	{
		err = deserialize(microseconds, ctx);
		if (err != errc::ok) return err;
	}

	output = (is_negative.value ? -1 : 1) * (
		 ::date::days(days.value) +
		 std::chrono::hours(hours.value) +
		 std::chrono::minutes(minutes.value) +
		 std::chrono::seconds(seconds.value) +
		 std::chrono::microseconds(microseconds.value)
	);
	return errc::ok;
}

inline std::pair<boost::mysql::error_code, std::uint8_t>
boost::mysql::detail::deserialize_message_type(
	deserialization_context& ctx
)
{
	int1 msg_type;
	std::pair<mysql::error_code, std::uint8_t> res {};
	auto err = deserialize(msg_type, ctx);
	if (err == errc::ok)
	{
		res.second = msg_type.value;
	}
	else
	{
		res.first = make_error_code(err);
	}
	return res;
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_SERIALIZATION_IPP_ */
