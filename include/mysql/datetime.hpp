#ifndef INCLUDE_MYSQL_DATETIME_HPP_
#define INCLUDE_MYSQL_DATETIME_HPP_

#include <mysql/datetime.hpp>
#include <cstdint>
#include <string_view>
#include <ostream>

namespace mysql
{

class datetime
{
	std::uint16_t year_ {};
	std::uint8_t month_ {};
	std::uint8_t day_ {};
	std::uint8_t hour_ {};
	std::uint8_t minute_ {};
	std::uint8_t second_ {};
	std::uint32_t microsecond_ {};
public:
	constexpr datetime() = default;
	constexpr datetime(std::uint16_t years, std::uint8_t month, std::uint8_t day,
			 std::uint8_t hour=0, std::uint8_t min=0, std::uint8_t seconds=0, std::uint32_t micro=0) noexcept;

	constexpr std::uint16_t year() const noexcept { return year_; }
	constexpr std::uint8_t month() const noexcept { return month_; }
	constexpr std::uint8_t day() const noexcept { return day_; }
	constexpr std::uint8_t hour() const noexcept { return hour_; }
	constexpr std::uint8_t minute() const noexcept { return minute_; }
	constexpr std::uint8_t second() const noexcept { return second_; }
	constexpr std::uint32_t microsecond() const noexcept { return microsecond_; }

	constexpr void set_year(std::uint16_t value) noexcept { year_ = value; }
	constexpr void set_month(std::uint8_t value) noexcept { month_ = value; }
	constexpr void set_day(std::uint8_t value) noexcept { day_ = value; }
	constexpr void set_hour(std::uint8_t value) noexcept { hour_ = value; }
	constexpr void set_minute(std::uint8_t value) noexcept { minute_ = value; }
	constexpr void set_second(std::uint8_t value) noexcept { second_ = value; }
	constexpr void set_microsecond(std::uint32_t value) noexcept { microsecond_ = value; }

	constexpr bool operator==(const datetime& rhs) const noexcept;
	constexpr bool operator!=(const datetime& rhs) const noexcept { return !(*this==rhs); }

	static constexpr std::size_t max_string_size = 4 + 2*5 + 6 + 6 + 1;
	bool from_string(std::string_view value);
	std::string to_string() const;
	void to_string(char* to) const noexcept;

	// TODO: provide compatibilitiy with some well-known system
};

std::ostream& operator<<(std::ostream& os, const datetime& value);

}

#include "mysql/impl/datetime_impl.hpp"

#endif /* INCLUDE_MYSQL_DATETIME_HPP_ */
