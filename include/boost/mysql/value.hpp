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
#include <optional>

/**
 * \defgroup values Values
 * \brief Classes and functions related to the representation
 * of database values. See boost::mysql::value for more info.
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
 * \details
 *
 * A boost::mysql::value is a variant-like class. At a given time,
 * it always holds a value of one of the type alternatives, henceforth
 * the stored value. See value::variant_type for the list of all
 * possible type alternatives. A value can be converted to an
 * actual variant using value::to_variant.
 *
 * NULL values are considered to hold the value nullptr, with actual
 * type std::nullptr_t. You can check for NULL values using
 * value::is_null. There is no distinction between NULL values
 * for different database types (e.g. a NULL value for a TINY
 * column is the same as a NULL value for a VARCHAR column).
 *
 * To query if a value holds a specific alternative, use value::is.
 * To retrieve the actual value, use value::get or value::get_optional.
 * For certain types, if the actual type is different than the
 * one passed to get/get_optional, these two will try to convert
 * the value to the requested type. The following conversions are
 * considered:
 *   - If the actual type is std::uint64_t, the requested type
 *     was std::int64_t, and the value is within the range
 *     of a std::int64_t, it will be converted to this type.
 *   - If the actual type is std::int64_t, the requested type
 *     was std::uint64_t, and the value is within the range
 *     of a std::uint64_t, it will be converted to this type.
 *   - If the actual type was float, and the requested type
 *     was double, it will be converted.
 *
 * The mapping from database types (e.g. TINY, VARCHAR...) to C++
 * types is not one to one. The following lists the mapping from database
 * types to C++ types, together with the ranges and specific considerations
 * for each one.
 *
 * - Integral types:
 *   - **TINYINT**. 1 byte integer type. If it is signed, it is represented
 *     as a std::int64_t, and its range is between -0x80 and 0x7f.
 *     If unsigned, represented as a std::uint64_t, and its range is
 *     between 0 and 0xff.
 *   - **SMALLINT**. 2 byte integer type. If it is signed, it is represented
 *     as a std::int64_t, and its range is between -0x8000 and 0x7fff.
 *     If unsigned, represented as a std::uint64_t, and its range is
 *     between 0 and 0xffff.
 *   - **MEDIUMINT**. 3 byte integer type. If it is signed, it is represented
 *     as a std::int64_t, and its range is between -0x800000 and 0x7fffff.
 *     If unsigned, represented as a std::uint64_t, and its range is
 *     between 0 and 0xffffff.
 *   - **INT**. 4 byte integer type. If it is signed, it is represented
 *     as a std::int64_t, and its range is between -0x80000000 and 0x7fffffff.
 *     If unsigned, represented as a std::uint64_t, and its range is
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
 * \warning This is a lightweight, cheap-to-copy class. Strings
 * are represented as string_views, not as owning strings.
 * This implies that if a value is a string, it will point
 * to a **externally owned** piece of memory (this will typically be
 * a boost::mysql::owning_row object).
 */
class value
{
public:
    /// Type of a variant representing the value.
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

    /// Constructs a NULL value.
    constexpr value() = default;

    /**
     * \brief Initialization constructor.
     * \details Initializes *this with the same type and value that
     * variant_type(v) would contain. The following exceptions apply:
     *   - If T is any unsigned integer, the type will be std::uint64_t
     *     and the value, std::uint64_t(v).
     *   - If T is any signed integer, the type will be std::int64t
     *     and the value, std::int64_t(v).
     *
     * Examples:
     *   - value(48) -> std::int64_t
     *   - value(std::uint8_t(2)) -> std::uint64_t
     *   - value("test") -> std::string_view
     */
    template <typename T>
    explicit constexpr value(const T& v) noexcept;

    /**
     * \brief Checks if the value is NULL.
     * \details Returns true only if the value's current type alternative
     * is std::nullptr_t. Equivalent to value::is<std::nullptr_t>().
     */
    constexpr bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(repr_); }

    /**
     * \brief Checks if the current type alternative is T.
     * \details T should be one of value::variant_type's type alternatives.
     * This function does *not* take into account possible type conversions
     * (e.g. float to double). It returns true only if the current alternative
     * matches T exactly. See value::is_convertible_to for a version of this
     * function taking conversions into account. This function is faster
     * than value::is_convertible_to.
     */
    template <typename T>
    constexpr bool is() const noexcept { return std::holds_alternative<T>(repr_); }

    /**
     * \brief Checks if the current value can be converted to T.
     * \details T should be one of value::variant_type's type alternatives.
     * This function returns true if the current type alternative is T (value::is<T>()
     * returns true) or if there exists a conversion from the current alternative
     * to T that does not cause loss of precision (e.g. float to double).
     * See boost::mysql::value for a list of such conversions.
     *
     * Use this function if you only need to know if a conversion is possible or not.
     * If you also need to access the stored value, use value::get_optional and check
     * the returned optional instead.
     */
    template <typename T>
    constexpr bool is_convertible_to() const noexcept { return get_optional<T>().has_value(); }

    /**
     * \brief Retrieves the stored value or throws an exception.
     * \details If the stored value is a T, or can be converted to T using
     * one of the conversions listed in boost::mysql::value's docs (i.e. when
     * value::is_convertible_to<T>() returns true), returns the converted value.
     * Otherwise throws std::bad_variant_access.
     *
     * \warning The following code pattern, where v is a boost::mysql::value,
     * is correct but inefficient:
     * \code
     * if (v.is_convertible_to<double>())
     * {
     *     double d = v.get<double>();
     *     // Do stuff with d
     * }
     * \endcode
     * Prefer the following:
     * \code
     * std::optional<double> d = v.get_optional<double>();
     * if (d)
     * {
     *     // Do stuff with d
     * }
     * \endcode
     */
    template <typename T>
    constexpr T get() const;

    /**
     * \brief Retrieves the stored value as an optional.
     * \details If the stored value is a T, or can be converted to T using
     * one of the conversions listed in boost::mysql::value's docs (i.e. when
     * value::is_convertible_to<T>() returns true), returns an optional
     * containing the converted value. Otherwise returns an empty optional.
     */
    template <typename T>
    constexpr std::optional<T> get_optional() const noexcept;

    /// Converts a value to an actual variant of type value::variant_type.
    variant_type to_variant() const noexcept { return repr_; }

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
