#ifndef INCLUDE_MYSQL_IMPL_DATETIME_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_DATETIME_IMPL_HPP_

#include <cstdio>
#include <cstring>
#include <cassert>

constexpr mysql::datetime::datetime(
	std::uint16_t years,
	std::uint8_t month,
	std::uint8_t day,
	std::uint8_t hour,
	std::uint8_t min,
	std::uint8_t seconds,
	std::uint32_t micro
) noexcept :
	year_(years),
	month_(month),
	day_(day),
	hour_(hour),
	minute_(min),
	second_(seconds),
	microsecond_(micro)
{
}

constexpr bool mysql::datetime::operator==(
	const datetime& rhs
) const noexcept
{
	return year_ == rhs.year_ &&
		   month_ == rhs.month_ &&
		   day_ == rhs.day_ &&
		   hour_ == rhs.hour_ &&
		   minute_ == rhs.minute_ &&
		   second_ == rhs.second_ &&
		   microsecond_ == rhs.microsecond_;
}

bool mysql::datetime::from_string(
	std::string_view value
)
{
	if (value.size() > max_string_size - 1) return false;
	char buffer [max_string_size] {};
	memcpy(buffer, value.data(), value.size());
	unsigned years {}, months {}, days {}, hours {}, mins {}, seconds {}, microseconds {};
	int elms_parsed = sscanf(buffer, "%u-%u-%u %u:%u:%u.%u",
			&years, &months, &days, &hours, &mins, &seconds, &microseconds);
	if (elms_parsed < 3 ||
	    months > 12 ||
		days > 31 ||
		hours > 24 ||
		mins > 60 ||
		seconds > 60 ||
		microseconds > 1000000)
	{
		return false;
	}
	year_ = years;
	month_ = months;
	day_ = days;
	hour_ = hours;
	minute_ = mins;
	second_ = seconds;
	microsecond_ = microseconds;
	return true;
}

inline void mysql::datetime::to_string(
	char* to
) const noexcept
{
	unsigned years = std::min(static_cast<unsigned>(year()), 9999u);
	unsigned months = std::min(month(), std::uint8_t(12));
	unsigned days = std::min(day(), std::uint8_t(31));
	unsigned hrs = std::min(hour(), std::uint8_t(24));
	unsigned mins = std::min(minute(), std::uint8_t(60));
	unsigned secs = std::min(second(), std::uint8_t(60));
	unsigned micros = std::min(microsecond(), 999999u);
	int result = snprintf(to, max_string_size, "%4u-%2u-%2u %2u:%2u:%2u.%6u",
			years, months, days, hrs, mins, secs, micros);
	assert(result == (max_string_size - 1));
}

inline std::string mysql::datetime::to_string() const
{
	std::string res (max_string_size, 0);
	to_string(res.data());
	res.pop_back();
	return res;
}

inline std::ostream& mysql::operator<<(
	std::ostream& os,
	const datetime& value
)
{
	char buffer [datetime::max_string_size];
	value.to_string(buffer);
	return os << buffer;
}

#endif /* INCLUDE_MYSQL_IMPL_DATETIME_IMPL_HPP_ */
