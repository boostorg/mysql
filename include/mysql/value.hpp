#ifndef MYSQL_ASIO_VALUE_HPP
#define MYSQL_ASIO_VALUE_HPP

#include <variant>
#include <cstdint>
#include <string_view>
#include <date/date.h>
#include <chrono>
#include <ostream>

namespace mysql
{

using date = ::date::sys_days;
using datetime = ::date::sys_time<std::chrono::microseconds>;
using time = std::chrono::microseconds;
using year = ::date::year;

using value = std::variant<
	std::int32_t,      // signed TINYINT, SMALLINT, MEDIUMINT, INT
	std::int64_t,      // signed BIGINT
	std::uint32_t,     // unsigned TINYINT, SMALLINT, MEDIUMINT, INT
	std::uint64_t,     // unsigned BIGINT
	std::string_view,  // CHAR, VARCHAR, BINARY, VARBINARY, TEXT (all sizes), BLOB (all sizes), ENUM, SET, DECIMAL, BIT, GEOMTRY
	float,             // FLOAT
	double,            // DOUBLE
	date,              // DATE
	datetime,          // DATETIME, TIMESTAMP
	time,              // TIME
	year,              // YEAR
	std::nullptr_t     // Any of the above when the value is NULL
>;

inline bool operator==(const value& lhs, const value& rhs);
inline bool operator!=(const value& lhs, const value& rhs) { return !(lhs == rhs); }

inline bool operator==(const std::vector<value>& lhs, const std::vector<value>& rhs);
inline bool operator!=(const std::vector<value>& lhs, const std::vector<value>& rhs) { return !(lhs == rhs); }

inline std::ostream& operator<<(std::ostream& os, const value& value);

}

#include "mysql/impl/value.hpp"


#endif