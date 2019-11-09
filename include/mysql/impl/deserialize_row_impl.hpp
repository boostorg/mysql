#ifndef INCLUDE_MYSQL_IMPL_DESERIALIZE_ROW_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_DESERIALIZE_ROW_IMPL_HPP_

#include <cstdlib>
#include <cmath>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <date/date.h>

namespace mysql
{
namespace detail
{

inline Error deserialize_text_value_impl(
	std::string_view from,
	date& to
)
{
	constexpr std::size_t size = 4 + 2 + 2 + 2; // year, month, day, separators
	if (from.size() != size) return Error::protocol_value_error;
	unsigned year, month, day;
	char buffer [size + 1] {};
	memcpy(buffer, from.data(), from.size());
	int parsed = sscanf(buffer, "%4u-%2u-%2u", &year, &month, &day);
	if (parsed != 3) return Error::protocol_value_error;
	::date::year_month_day result (::date::year(year)/::date::month(month)/::date::day(day));
	if (!result.ok()) return Error::protocol_value_error;
	if (result > max_date || result < min_date) return Error::protocol_value_error;
	to = result;
	return Error::ok;
}

inline Error deserialize_text_value_impl(
	std::string_view from,
	time& to,
	unsigned decimals
)
{
	// size check
	constexpr std::size_t min_size = 2 + 2 + 2 + 2; // hours, mins, seconds, no micros
	constexpr std::size_t max_size = min_size + 1 + 7; // hour extra character and micros
	decimals = std::min(decimals, 6u);
	if (from.size() < min_size || from.size() > max_size) return Error::protocol_value_error;

	// Parse it
	int hours;
	unsigned minutes, seconds, micros = 0;
	char buffer [max_size + 1] {};
	memcpy(buffer, from.data(), from.size());
	int parsed = decimals ? sscanf(buffer, "%3d:%2u:%2u.%6u", &hours, &minutes, &seconds, &micros) :
			                sscanf(buffer, "%3d:%2u:%2u", &hours, &minutes, &seconds);
	if ((decimals && parsed != 4) || parsed != 3) return Error::protocol_value_error;
	micros *= std::pow(10, 6 - decimals);
	bool is_negative = hours < 0;
	hours = std::abs(hours);

	// Sum it
	auto res = std::chrono::hours(hours) + std::chrono::minutes(minutes) +
			   std::chrono::seconds(seconds) + std::chrono::microseconds(micros);
	if (is_negative)
	{
		res = -res;
	}

	// Range check
	if (res > max_time || res < min_time) return Error::protocol_value_error;

	to = res;
	return Error::ok;
}

inline Error deserialize_text_value_impl(
	std::string_view from,
	datetime& to,
	unsigned decimals
)
{
	// Length check
	constexpr std::size_t min_size = 4 + 5*2 + 5; // year, month, day, hour, minute, seconds, separators
	constexpr std::size_t max_size = min_size + 7;
	decimals = std::min(decimals, 6u);
	std::size_t expected_size = min_size + (decimals ? decimals + 1 : 0);
	if (from.size() != expected_size) return Error::protocol_value_error;

	// Date part
	date dt;
	auto err = deserialize_text_value_impl(from.substr(0, 10), dt);
	if (err != Error::ok) return err;

	// Time of day part
	time time_of_day;
	err = deserialize_text_value_impl(from.substr(11), time_of_day, decimals);
	if (err != Error::ok) return err;
	constexpr auto max_time_of_day = std::chrono::hours(24) - std::chrono::microseconds(1);
	if (time_of_day < std::chrono::seconds(0) || time_of_day > max_time_of_day) return Error::protocol_value_error;

	// Sum it up
	to = dt + time_of_day;
	return Error::ok;
}


template <typename T>
std::enable_if_t<std::is_arithmetic_v<T>, Error>
deserialize_text_value_impl(std::string_view from, T& to)
{
	bool ok = boost::conversion::try_lexical_convert(from.data(), from.size(), to);
	return ok ? Error::ok : Error::protocol_value_error;
}

Error deserialize_text_value_impl(std::string_view from, std::string_view& to)
{
	to = from;
	return Error::ok;
}

Error deserialize_text_value_impl(std::string_view from, std::nullptr_t& to)
{
	return Error::ok;
}

Error deserialize_text_value_impl(std::string_view from, year& to)
{
	int value;
	auto err = deserialize_text_value_impl(from, value);
	if (err != Error::ok) return err;
	to = year(value);
	return to.ok() ? Error::ok : Error::protocol_value_error;
}

template <typename T, typename... Args>
Error deserialize_text_value_to_variant(std::string_view from, value& to, Args&&... args)
{
	T value;
	auto err = deserialize_text_value_impl(from, value, std::forward<Args>(args)...);
	if (err == Error::ok)
	{
		to = value;
	}
	return err;
}

}
}

inline mysql::Error mysql::detail::deserialize_text_value(
	std::string_view from,
	const field_metadata& meta,
	value& output
)
{
	switch (meta.type())
	{
    case field_type::decimal:
    case field_type::varchar:
    case field_type::bit:
    case field_type::newdecimal:
    case field_type::enum_:
    case field_type::set:
    case field_type::tiny_blob:
    case field_type::medium_blob:
    case field_type::long_blob:
    case field_type::blob:
    case field_type::var_string:
    case field_type::string:
    case field_type::geometry:
    	return deserialize_text_value_to_variant<std::string_view>(from, output);
    case field_type::tiny:
    case field_type::short_:
    case field_type::int24:
    case field_type::long_:
    	return meta.is_unsigned() ?
    			deserialize_text_value_to_variant<std::uint32_t>(from, output) :
				deserialize_text_value_to_variant<std::int32_t>(from, output);
    case field_type::longlong:
    	return meta.is_unsigned() ?
    			deserialize_text_value_to_variant<std::uint64_t>(from, output) :
				deserialize_text_value_to_variant<std::int64_t>(from, output);
    case field_type::float_:
    	return deserialize_text_value_to_variant<float>(from, output);
    case field_type::double_:
    	return deserialize_text_value_to_variant<double>(from, output);
    case field_type::null:
    	return deserialize_text_value_to_variant<nullptr_t>(from, output);
    case field_type::timestamp:
    case field_type::datetime:
    	return deserialize_text_value_to_variant<datetime>(from, output, meta.decimals());
    case field_type::date:
    	return deserialize_text_value_to_variant<date>(from, output);
    case field_type::time:
    	return deserialize_text_value_to_variant<time>(from, output, meta.decimals());
    case field_type::year:
    	return deserialize_text_value_to_variant<year>(from, output);
    default:
    	return Error::protocol_value_error;
	}
}


template <typename Allocator>
mysql::error_code mysql::detail::deserialize_text_row(
	DeserializationContext& ctx,
	const resultset_metadata<Allocator>& meta,
	std::vector<value>& output
)
{
	output.resize(meta.fields().size());
	for (std::vector<value>::size_type i = 0; i < meta.fields().size(); ++i)
	{
		string_lenenc value_str;
		Error err = deserialize(value_str, ctx);
		if (err) return make_error_code(err);
		err = parse_text_value(value_str.value, meta.fields()[i], output[i]);
		if (err) return make_error_code(err);
	}
	if (!ctx.empty()) return make_error_code(Error::extra_bytes);
	return error_code();
}



#endif /* INCLUDE_MYSQL_IMPL_DESERIALIZE_ROW_IMPL_HPP_ */
