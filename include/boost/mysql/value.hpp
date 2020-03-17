#ifndef MYSQL_ASIO_VALUE_HPP
#define MYSQL_ASIO_VALUE_HPP

#include <variant>
#include <cstdint>
#include <string_view>
#include <date/date.h>
#include <chrono>
#include <ostream>
#include <array>
#include <vector>

namespace mysql
{

/// Type representing MySQL DATE data type.
using date = ::date::sys_days;

/// Type representing MySQL DATETIME and TIMESTAMP data types.
using datetime = ::date::sys_time<std::chrono::microseconds>;

/// Type representing MySQL TIME data type.
using time = std::chrono::microseconds;

/**
 * \brief Represents a value in the database of any of the allowed types.
 * \details If a value is NULL, the type of the variant will be nullptr_t.
 *
 * If a value is a string, the type will be string_view, and it will
 * point to a externally owned piece of memory. That implies that mysql::value
 * does **NOT** own its string memory (saving copies).
 *
 * MySQL types BIT and GEOMETRY do not have direct support yet.
 * They are represented as binary strings.
 */
using value = std::variant<
	std::int32_t,      // signed TINYINT, SMALLINT, MEDIUMINT, INT
	std::int64_t,      // signed BIGINT
	std::uint32_t,     // unsigned TINYINT, SMALLINT, MEDIUMINT, INT, YEAR
	std::uint64_t,     // unsigned BIGINT
	std::string_view,  // CHAR, VARCHAR, BINARY, VARBINARY, TEXT (all sizes), BLOB (all sizes), ENUM, SET, DECIMAL, BIT, GEOMTRY
	float,             // FLOAT
	double,            // DOUBLE
	date,              // DATE
	datetime,          // DATETIME, TIMESTAMP
	time,              // TIME
	std::nullptr_t     // Any of the above when the value is NULL
>;

/// Tests for equality (type and value).
inline bool operator==(const value& lhs, const value& rhs);

/// Tests for inequality (type and value).
inline bool operator!=(const value& lhs, const value& rhs) { return !(lhs == rhs); }

inline bool operator==(const std::vector<value>& lhs, const std::vector<value>& rhs);
inline bool operator!=(const std::vector<value>& lhs, const std::vector<value>& rhs) { return !(lhs == rhs); }

/// Streams a value.
inline std::ostream& operator<<(std::ostream& os, const value& value);

/**
 * \brief Creates an array of mysql::value out of the passed in arguments.
 * \details Each argument creates an element in the array. It should be possible
 * to construct a mysql::value out of every single argument passed in.
 */
template <typename... Types>
std::array<value, sizeof...(Types)> make_values(Types&&... args);

}

#include "boost/mysql/impl/value.hpp"


#endif
