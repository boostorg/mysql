//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_VALUE_HPP
#define BOOST_MYSQL_VALUE_HPP

#include <cstdint>
#include <boost/utility/string_view.hpp>
#include <chrono>
#include <ostream>
#include <array>
#include <exception>
#include <boost/variant2/variant.hpp>
#include <boost/optional/optional.hpp>
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif

namespace boost {
namespace mysql {

/**
 * \brief Duration representing a day (24 hours).
 * \details Suitable to represent the range of dates MySQL offers.
 * May differ in representation from `std::chrono::days` in C++20.
 */
using days = std::chrono::duration<int, std::ratio<3600 * 24>>;

/// Type representing MySQL `__DATE__` data type.
using date = std::chrono::time_point<std::chrono::system_clock, days>;

/// Type representing MySQL `__DATETIME__` and `__TIMESTAMP__` data types.
using datetime = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;

/// Type representing MySQL `__TIME__` data type.
using time = std::chrono::microseconds;

/// Monostate type representing a NULL value.
using null_t = boost::variant2::monostate;

/// Exception type thrown when trying to access a [reflink value] with an incorrect type.
class bad_value_access : public std::exception
{
public:
    const char* what() const noexcept override { return "bad_value_access"; }
};

/**
 * \brief Represents a value in the database of any of the allowed types.
 *        See [link mysql.values this section] for more info.
 * \details
 *
 * A [reflink value] is a variant-like class. At a given time,
 * it always holds a value of one of the type alternatives.
 * See [link mysql.values this section] for information
 * on how to use this class.
 *
 * This is a lightweight, cheap-to-copy class. Strings
 * are represented as string_views, not as owning strings.
 * This implies that if a value is a string, it will point
 * to a [*externally owned] piece of memory. See
 * [link mysql.values.strings this section] for more info.
 */
class value
{
public:
    /// Type of a variant representing the value.
    using variant_type = boost::variant2::variant<
        null_t,            // Any of the below when the value is NULL
        std::int64_t,      // signed TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT
        std::uint64_t,     // unsigned TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT, YEAR
        boost::string_view,// CHAR, VARCHAR, BINARY, VARBINARY, TEXT (all sizes), BLOB (all sizes), ENUM, SET, DECIMAL, BIT, GEOMTRY
        float,             // FLOAT
        double,            // DOUBLE
        date,              // DATE
        datetime,          // DATETIME, TIMESTAMP
        time               // TIME
    >;

    /// Constructs a NULL value.
    BOOST_CXX14_CONSTEXPR value() = default;

    /**
      * \brief Constructs a NULL value.
      * \details
      * Caution: `value(NULL)` will __NOT__ match this overload. It will try to construct
      * a `boost::string_view` from a NULL C string, causing undefined behavior.
      */
    BOOST_CXX14_CONSTEXPR value(std::nullptr_t) noexcept : repr_(null_t()) {}

    /**
     * \brief Initialization constructor.
     * \details Initializes `*this` with the same type and value that
     * `variant_type(v)` would contain. The following exceptions apply:
     *
     *   - If `T` is any unsigned integer, the type will be `std::uint64_t`
     *     and the value, `std::uint64_t(v)`.
     *   - If `T` is any signed integer, the type will be `std::int64_t`
     *     and the value, `std::int64_t(v)`.
     *
     * Examples:
     *
     *  - `value(48)` yields `std::int64_t`.
     *  - `value(std::uint8_t(2))` yields `std::uint64_t`.
     *  - `value("test")` yields `boost::string_view`.
     */
    template <class T>
    explicit BOOST_CXX14_CONSTEXPR value(const T& v) noexcept;

    /**
     * \brief Checks if the value is NULL.
     * \details Returns `true` only if the value's current type alternative
     * is `std::nullptr_t`. Equivalent to `value::is<null_t>()`.
     */
    BOOST_CXX14_CONSTEXPR bool is_null() const noexcept
    {
        return boost::variant2::holds_alternative<null_t>(repr_);
    }

    /**
     * \brief Checks if the current type alternative is `T`.
     * \details `T` should be one of [refmem value variant_type]'s type alternatives.
     * This function does [*not] take into account the [link mysql.values.conversions
     * possible type conversions]. It returns `true` only if the current alternative
     * matches `T` exactly. See [refmem value is_convertible_to] for a version of this
     * function taking [link mysql.values.conversions conversions]
     * into account. This function is faster than [refmem value is_convertible_to].
     */
    template <class T>
    BOOST_CXX14_CONSTEXPR bool is() const noexcept
    {
        return boost::variant2::holds_alternative<T>(repr_);
    }

    /**
     * \brief Checks if the current value can be converted to `T`.
     * \details `T` should be one of [refmem value variant_type]'s type alternatives.
     * This function returns `true` if the current type alternative is `T` (`value::is<T>()`
     * returns `true`) or if there exists a conversion from the current alternative
     * to `T` that does not cause loss of precision (e.g. `float` to `double`).
     * See [link mysql.values.conversions
     * this section] for more information about conversions.
     *
     * Use this function if you only need to know if a conversion is possible or not.
     * If you also need to access the stored value, use [refmem value get_optional] or
     * [refmem value get_std_optional] and check the returned optional instead.
     */
    template <class T>
    BOOST_CXX14_CONSTEXPR bool is_convertible_to() const noexcept;

    /**
     * \brief Retrieves the stored value or throws an exception.
     * \details `T` should be one of [refmem value variant_type]'s type alternatives.
     * If the stored value is a `T`, or can be converted to `T` using
     * one of [link mysql.values.conversions the allowed conversions],
     * returns the converted value. Otherwise throws
     * [reflink bad_value_access].
     */
    template <class T>
    T get() const;

    /**
     * \brief Retrieves the stored value as a __boost_optional__.
     * \details `T` should be one of [refmem value variant_type]'s type alternatives.
     * If the stored value is a `T`, or can be converted to `T` using
     * one of [link mysql.values.conversions the allowed conversions],
     * returns an optional containing the converted value.
     * Otherwise returns an empty optional.
     */
    template <class T>
    boost::optional<T> get_optional() const noexcept;

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    /**
     * \brief Retrieves the stored value as a `std::optional`.
     * \details `T` should be one of [refmem value variant_type]'s type alternatives.
     * If the stored value is a `T`, or can be converted to `T` using
     * one of [link mysql.values.conversions the allowed conversions],
     * returns an optional containing the converted value.
     * Otherwise returns an empty optional.
     */
    template <class T>
    constexpr std::optional<T> get_std_optional() const noexcept;
#endif

    /// Converts a value to an actual variant of type [refmem value variant_type].
    BOOST_CXX14_CONSTEXPR variant_type to_variant() const noexcept { return repr_; }

    /// Tests for equality (type and value); see [link mysql.values.relational this section] for more info.
    BOOST_CXX14_CONSTEXPR bool operator==(const value& rhs) const noexcept { return repr_ == rhs.repr_; }

    /// Tests for inequality (type and value); see [link mysql.values.relational this section] for more info.
    BOOST_CXX14_CONSTEXPR bool operator!=(const value& rhs) const noexcept { return !(*this==rhs); }

    /// Tests for inequality (type and value); see [link mysql.values.relational this section] for more info.
    BOOST_CXX14_CONSTEXPR bool operator<(const value& rhs) const noexcept { return repr_ < rhs.repr_; }

    /// Tests for inequality (type and value); see [link mysql.values.relational this section] for more info.
    BOOST_CXX14_CONSTEXPR bool operator<=(const value& rhs) const noexcept { return repr_ <= rhs.repr_; }

    /// Tests for inequality (type and value); see [link mysql.values.relational this section] for more info.
    BOOST_CXX14_CONSTEXPR bool operator>(const value& rhs) const noexcept { return repr_ > rhs.repr_; }

    /// Tests for inequality (type and value); see [link mysql.values.relational this section] for more info.
    BOOST_CXX14_CONSTEXPR bool operator>=(const value& rhs) const noexcept { return repr_ >= rhs.repr_; }
private:
    struct unsigned_int_tag {};
    struct signed_int_tag {};
    struct no_tag {};

    BOOST_CXX14_CONSTEXPR value(std::uint64_t val, unsigned_int_tag) noexcept : repr_(val) {}
    BOOST_CXX14_CONSTEXPR value(std::int64_t val, signed_int_tag) noexcept : repr_(val) {}

    template <class T>
    BOOST_CXX14_CONSTEXPR value(const T& val, no_tag) noexcept : repr_(val) {}

    variant_type repr_;
};

/**
 * \relates value
 * \brief Streams a value.
 * \details The value should be in the MySQL valid range of values. Concretely,
 * if the value is a [reflink date], [reflink datetime] or
 * [reflink time], it should be in the
 * \\[[reflink min_date], [reflink max_date]\\],
 * \\[[reflink min_datetime], [reflink max_datetime]\\] or
 * \\[[reflink min_time], [reflink max_time]\\], respectively.
 * Otherwise, the results are undefined.
 */
inline std::ostream& operator<<(std::ostream& os, const value& v);


/**
 * \relates value
 * \brief Creates an array of [reflink value]s out of the passed in arguments.
 * \details Each argument creates an element in the array. It should be possible
 * to construct a [reflink value] out of every single argument passed in.
 */
template <class... Types>
BOOST_CXX14_CONSTEXPR std::array<value, sizeof...(Types)> make_values(Types&&... args);

/// The minimum allowed value for [reflink date] (0000-01-01).
BOOST_CXX14_CONSTEXPR const date min_date { days(-719528) };

/// The maximum allowed value for [reflink date] (9999-12-31).
BOOST_CXX14_CONSTEXPR const date max_date { days(2932896) };

/// The minimum allowed value for [reflink datetime].
BOOST_CXX14_CONSTEXPR const datetime min_datetime = min_date;

/// The maximum allowed value for [reflink datetime].
BOOST_CXX14_CONSTEXPR const datetime max_datetime = max_date + std::chrono::hours(24) - std::chrono::microseconds(1);

/// The minimum allowed value for [reflink time].
constexpr time min_time = -std::chrono::hours(839);

/// The maximum allowed value for [reflink time].
constexpr time max_time = std::chrono::hours(839);

} // mysql
} // boost

#include "boost/mysql/impl/value.hpp"


#endif
