//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_VALUE_HPP
#define BOOST_MYSQL_VALUE_HPP

#include <variant>
#include <cstdint>
#include <string_view>
#include <date/date.h>
#include <chrono>
#include <ostream>
#include <array>
#include <vector>
#include "boost/mysql/detail/auxiliar/tmp.hpp"

/**
 * \defgroup values Values
 * \brief Classes and functions related to the representation
 * of database values.
 *
 * In a MySQL table, a value can be any of the MySQL supported types.
 * In this library, table rows are represented as a collection of
 * boost::mysql::value, which is a union of all the supported types.
 *
 * The following lists all the types known to Boost.Mysql, together with
 * the ranges and specific considerations for each one.
 *
 * - Integral types:
 *   - **TINYINT**. 1 byte integer type. If it is signed, it is represented
 *     as a std::int32_t, and its range is between -0x80 and 0x7f.
 *     If unsigned, represented as a std::uint32_t, and its range is
 *     between 0 and 0xff.
 *   - **SMALLINT**. 2 byte integer type. If it is signed, it is represented
 *     as a std::int32_t, and its range is between -0x8000 and 0x7fff.
 *     If unsigned, represented as a std::uint32_t, and its range is
 *     between 0 and 0xffff.
 *   - **MEDIUMINT**. 3 byte integer type. If it is signed, it is represented
 *     as a std::int32_t, and its range is between -0x800000 and 0x7fffff.
 *     If unsigned, represented as a std::uint32_t, and its range is
 *     between 0 and 0xffffff.
 *   - **INT**. 4 byte integer type. If it is signed, it is represented
 *     as a std::int32_t, and its range is between -0x80000000 and 0x7fffffff.
 *     If unsigned, represented as a std::uint32_t, and its range is
 *     between 0 and 0xffffffff.
 *   - **BIGINT**. 8 byte integer type. If it is signed, it is represented
 *     as a std::int64_t, and its range is between -0x8000000000000000 and
 *     0x7fffffffffffffff. If unsigned, represented as a std::uint64_t,
 *     and its range is between 0 and 0xffffffffffffffff.
 *   - **YEAR**. 1 byte integer type used to represent years. Its range is
 *     [1901, 2155], plus zero. Zero is often employed to represent
 *     invalid year values. We represent zero year here as a numeric 0.
 *     YEAR is represented as a std::uint32_t.
 * - Floating point types:
 *   - **FLOAT**. 4 byte floating point type. Represented as float.
 *   - **DOUBLE**. 8 byte floating point type. Represented as double.
 * - Date and time types:
 *   - **DATE**. Represented as a boost::mysql::date. All dates retrieved from
 *     the database are guaranteed to be in the range [boost::mysql::min_date,
 *     boost::mysql::max_date]. Note that these limits are slightly more
 *     flexible than the official DATE limits.
 *
 *     If sql_mode is set to ALLOW_INVALID_DATES, MySQL will accept
 *     invalid dates, like '2010-02-31'. Furthermore, if strict SQL mode
 *     is not enabled, MySQL will accept zero dates, like '0000-00-00', and
 *     dates with zero components, like '2010-00-20'. These dates are invalid
 *     and not representable as a boost::mysql::date. In this library, they are
 *     all represented as NULL values, instead (std::nullptr_t type). These values
 *     can be retrieved from the database in both text queries and prepared
 *     statements, but cannot be specified as parameters of prepared statements.
 *   - **DATETIME**. MySQL representation of a time point without time zone,
 *     with a resolution of one microsecond.
 *     Represented as a boost::mysql::datetime. All datetimes retrieved from
 *     the database are guaranteed to be in the range [boost::mysql::min_datetime,
 *     boost::mysql::max_datetime].
 *
 *     If sql_mode is set to ALLOW_INVALID_DATES, MySQL will accept
 *     datetimes with invalid dates, like '2010-02-31 10:10:10'.
 *     Furthermore, if strict SQL mode is not enabled, MySQL will accept
 *     zero datetimes, like '0000-00-00 00:00:00', and
 *     datetimes with zero date components, like '2010-00-20 00:00:00'.
 *     These datetimes are invalid because they do not represent any real time point,
 *     and are thus not representable as a boost::mysql::datetime. In this library, they are
 *     all represented as NULL values, instead (std::nullptr_t type). These values
 *     can be retrieved from the database in both text queries and prepared
 *     statements, but cannot be specified as parameters of prepared statements.
 *   - **TIMESTAMP**. Like DATETIME, it also represents a time point. When inserted,
 *     TIMESTAMPs are interpreted as local times, according to the variable time_zone,
 *     and converted to UTC for storage. When retrieved, they are converted back
 *     to the time zone indicated by time_zone. The retrieved value of a TIMESTAMP
 *     field is thus a time point in some local time zone, dictated by the current
 *     time_zone variable. As this variable can be changed programmatically, without
 *     the client knowing it, we represent TIMESTAMPs without their time zone,
 *     using boost::mysql::datetime. TIMESTAMP's range is narrower than DATETIME's,
 *     but we do not enforce it in the client.
 *
 *     If strict SQL mode is not enabled, MySQL accepts zero TIMESTAMPs, like
 *     '0000-00-00 00:00:00'. These timestamps are invalid because
 *     they do not represent any real time point, and are thus not representable
 *     as a boost::mysql::datetime. In this library, they are
 *     all represented as NULL values, instead (std::nullptr_t type). These values
 *     can be retrieved from the database in both text queries and prepared
 *     statements, but cannot be specified as parameters of prepared statements.
 *
 *   - **TIME**. A signed time duration, with a resolution of one microsecond.
 *     Represented as a boost::mysql::time (alias for std::chrono::microseconds).
 *     Guaranteed to be in range [boost::mysql::min_time and boost::mysql::max_time].
 * - String types. All text, character and blob types are represented as a
 *   std::string_view. Furthermore, any type without a more specialized representation
 *   is exposed as a std::string_view. Character strings are NOT aware of encoding -
 *   they are represented as the string raw bytes. The encoding for each character
 *   string column is part of the column metadata, and can be accessed using
 *   boost::mysql::field_metadata::character_set().
 *
 *   The following types are represented as strings in Boost.Mysql:
 *     - **CHAR**. Fixed-size character string.
 *     - **BINARY**. Fixed-size blob.
 *     - **VARCHAR**. Variable size character string with a maximum size.
 *     - **VARBINARY**. Variable size blob with a maximum size.
 *     - **TEXT** (all sizes). Variable size character string.
 *     - **BLOB** (all sizes). Variable size blob.
 *     - **ENUM**. Character string with a fixed set of possible values (only one possible).
 *     - **SET**. Character string with a fixed set of possible values (many possible).
 *
 *   The following types are not strings per se, but are represented as such because
 *   no better representation for them is available at the moment:
 *     - **DECIMAL**. A fixed precision numeric value. In this case, the string will contain
 *       the textual representation of the number (e.g. the string "20.52" for 20.52).
 *     - **NUMERIC**. Alias for DECIMAL.
 *     - **BIT**. A bitset between 1 and 64 bits wide. In this case, the string will contain
 *       the binary representation of the bitset.
 *     - **GEOMETRY**. In this case, the string will contain
 *       the binary representation of the geometry type.
 *
 */

namespace boost {
namespace mysql {

/**
 * \ingroup values
 * \brief Type representing MySQL DATE data type.
 */
using date = ::date::sys_days;

/**
 * \ingroup values
 * \brief Type representing MySQL DATETIME and TIMESTAMP data types.
 */
using datetime = ::date::sys_time<std::chrono::microseconds>;

/**
 * \ingroup values
 * \brief Type representing MySQL TIME data type.
 */
using time = std::chrono::microseconds;

/**
 * \ingroup values
 * \brief Represents a value in the database of any of the allowed types.
 * \details If a value is NULL, the type of the variant will be nullptr_t.
 *
 * If a value is a string, the type will be string_view, and it will
 * point to a externally owned piece of memory. That implies that boost::mysql::value
 * does **NOT** own its string memory (saving copies).
 *
 * MySQL types BIT and GEOMETRY do not have direct support yet.
 * They are represented as binary strings.
 */
class value
{
public:
    // The underlying representation
    using variant_type = std::variant<
        std::nullptr_t,    // Any of the below when the value is NULL
        std::int64_t,      // signed TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT
        std::uint64_t,     // unsigned TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT, YEAR
        std::string_view,  // CHAR, VARCHAR, BINARY, VARBINARY, TEXT (all sizes), BLOB (all sizes), ENUM, SET, DECIMAL, BIT, GEOMTRY
        float,             // FLOAT
        double,            // DOUBLE
        date,              // DATE
        datetime,          // DATETIME, TIMESTAMP
        time               // TIME
    >;

    // Default constructor: makes it a NULL value
    constexpr value() = default;

    // Initialization constructor accepting any of the variant alternatives
    template <typename T>
    explicit constexpr value(const T& v) noexcept;

    // Tests for NULL
    constexpr bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(repr_); }

    // Returns true if the stored value is T or can be converted to T without loss of precision
    template <typename T>
    constexpr bool is() const noexcept { return std::holds_alternative<T>(repr_); }

    template <typename T>
    constexpr bool is_convertible_to() const noexcept { return get_optional<T>().has_value(); }

    // Retrieves the stored value. If the stored value is not a T or cannot
    // be converted to T without loss of precision, throws.
    template <typename T>
    constexpr T get() const;

    // Retrieves the stored value, as an optional. If the stored value is not a T or cannot
    // be converted to T without loss of precision, returns an empty optional.
    template <typename T>
    constexpr std::optional<T> get_optional() const noexcept;

    // Returns the underlying variant type
    constexpr variant_type to_variant() const noexcept { return repr_; }

    /// Tests for equality (type and value).
    constexpr bool operator==(const value& rhs) const noexcept { return repr_ == rhs.repr_; }

    /// Tests for inequality (type and value).
    constexpr bool operator!=(const value& rhs) const noexcept { return !(*this==rhs); }
private:
    variant_type repr_;
};

/**
 * \brief Streams a value.
 * \relates value
 */
inline std::ostream& operator<<(std::ostream& os, const value& v);


/**
 * \brief Creates an array of mysql::value out of the passed in arguments.
 * \details Each argument creates an element in the array. It should be possible
 * to construct a mysql::value out of every single argument passed in.
 * \relates value
 */
template <typename... Types>
std::array<value, sizeof...(Types)> make_values(Types&&... args);

/// The minimum allowed value for boost::mysql::date.
constexpr date min_date = ::date::day(1)/::date::January/::date::year(0); // slightly more flexible than official min

/// The maximum allowed value for boost::mysql::date.
constexpr date max_date = ::date::day(31)/::date::December/::date::year(9999);

/// The minimum allowed value for boost::mysql::datetime.
constexpr datetime min_datetime = min_date;

/// The maximum allowed value for boost::mysql::datetime.
constexpr datetime max_datetime = max_date + std::chrono::hours(24) - std::chrono::microseconds(1);

/// The minimum allowed value for boost::mysql::time.
constexpr time min_time = -std::chrono::hours(839);

/// The maximum allowed value for boost::mysql::time.
constexpr time max_time = std::chrono::hours(839);

} // mysql
} // boost

#include "boost/mysql/impl/value.hpp"


#endif
