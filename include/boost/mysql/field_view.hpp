//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FIELD_VIEW_HPP
#define BOOST_MYSQL_FIELD_VIEW_HPP

#include <boost/utility/string_view.hpp>
#include <boost/variant2/variant.hpp>
#include <boost/mysql/detail/auxiliar/string_view_offset.hpp>
#include <cstdint>
#include <chrono>
#include <limits>
#include <ostream>
#include <array>
#include <exception>


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

/// Exception type thrown when trying to access a [reflink value] with an incorrect type.
class bad_field_access : public std::exception
{
public:
    const char* what() const noexcept override { return "bad_value_access"; }
};

enum class field_kind
{
    // Order here is important
    null = 0,
    int64,
    uint64,
    string,
    float_,
    double_,
    date,
    datetime,
    time
};

inline std::ostream& operator<<(std::ostream& os, field_kind v);

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
class field_view
{
public:
    /// Constructs a NULL value.
    BOOST_CXX14_CONSTEXPR field_view() = default;

    /**
      * \brief Constructs a NULL value.
      * \details
      * Caution: `value(NULL)` will __NOT__ match this overload. It will try to construct
      * a `boost::string_view` from a NULL C string, causing undefined behavior.
      */
    BOOST_CXX14_CONSTEXPR field_view(std::nullptr_t) noexcept : repr_(null_t()) {}

    BOOST_CXX14_CONSTEXPR field_view(signed char v) noexcept : repr_(std::int64_t(v)) {}
    BOOST_CXX14_CONSTEXPR field_view(short v) noexcept : repr_(std::int64_t(v)) {}
    BOOST_CXX14_CONSTEXPR field_view(int v) noexcept : repr_(std::int64_t(v)) {}
    BOOST_CXX14_CONSTEXPR field_view(long v) noexcept : repr_(std::int64_t(v)) {}
    BOOST_CXX14_CONSTEXPR field_view(long long v) noexcept : repr_(std::int64_t(v)) {}

    BOOST_CXX14_CONSTEXPR field_view(unsigned char v) noexcept : repr_(std::uint64_t(v)) {}
    BOOST_CXX14_CONSTEXPR field_view(unsigned short v) noexcept : repr_(std::uint64_t(v)) {}
    BOOST_CXX14_CONSTEXPR field_view(unsigned int v) noexcept : repr_(std::uint64_t(v)) {}
    BOOST_CXX14_CONSTEXPR field_view(unsigned long v) noexcept : repr_(std::uint64_t(v)) {}
    BOOST_CXX14_CONSTEXPR field_view(unsigned long long v) noexcept : repr_(std::uint64_t(v)) {}

    BOOST_CXX14_CONSTEXPR field_view(boost::string_view v) noexcept : repr_(v) {}
    BOOST_CXX14_CONSTEXPR field_view(float v) noexcept : repr_(v) {}
    BOOST_CXX14_CONSTEXPR field_view(double v) noexcept : repr_(v) {}
    BOOST_CXX14_CONSTEXPR field_view(const date& v) noexcept : repr_(v) {}
    BOOST_CXX14_CONSTEXPR field_view(const datetime& v) noexcept : repr_(v) {}
    BOOST_CXX14_CONSTEXPR field_view(const time& v) noexcept : repr_(v) {}

    // TODO: hide this
    BOOST_CXX14_CONSTEXPR field_view(detail::string_view_offset v) noexcept : repr_(v) {}

    BOOST_CXX14_CONSTEXPR inline field_kind kind() const noexcept;

    BOOST_CXX14_CONSTEXPR bool is_null() const noexcept { return kind() == field_kind::null; }
    BOOST_CXX14_CONSTEXPR bool is_int64() const noexcept { return kind() == field_kind::int64; }
    BOOST_CXX14_CONSTEXPR bool is_uint64() const noexcept { return kind() == field_kind::uint64; }
    BOOST_CXX14_CONSTEXPR bool is_string() const noexcept { return kind() == field_kind::string; }
    BOOST_CXX14_CONSTEXPR bool is_float() const noexcept { return kind() == field_kind::float_; }
    BOOST_CXX14_CONSTEXPR bool is_double() const noexcept { return kind() == field_kind::double_; }
    BOOST_CXX14_CONSTEXPR bool is_date() const noexcept { return kind() == field_kind::date; }
    BOOST_CXX14_CONSTEXPR bool is_datetime() const noexcept { return kind() == field_kind::datetime; }
    BOOST_CXX14_CONSTEXPR bool is_time() const noexcept { return kind() == field_kind::time; }

    BOOST_CXX14_CONSTEXPR const std::int64_t* if_int64() const noexcept { return boost::variant2::get_if<std::int64_t>(&repr_); }
    BOOST_CXX14_CONSTEXPR const std::uint64_t* if_uint64() const noexcept { return boost::variant2::get_if<std::uint64_t>(&repr_); }
    BOOST_CXX14_CONSTEXPR const boost::string_view* if_string() const noexcept { return boost::variant2::get_if<boost::string_view>(&repr_); }
    BOOST_CXX14_CONSTEXPR const float* if_float() const noexcept { return boost::variant2::get_if<float>(&repr_); }
    BOOST_CXX14_CONSTEXPR const double* if_double() const noexcept { return boost::variant2::get_if<double>(&repr_); }
    BOOST_CXX14_CONSTEXPR const date* if_date() const noexcept { return boost::variant2::get_if<date>(&repr_); }
    BOOST_CXX14_CONSTEXPR const datetime* if_datetime() const noexcept { return boost::variant2::get_if<datetime>(&repr_); }
    BOOST_CXX14_CONSTEXPR const time* if_time() const noexcept { return boost::variant2::get_if<time>(&repr_); }

    BOOST_CXX14_CONSTEXPR const std::int64_t& as_int64() const { return internal_as<std::int64_t>(); }
    BOOST_CXX14_CONSTEXPR const std::uint64_t& as_uint64() const { return internal_as<std::uint64_t>(); }
    BOOST_CXX14_CONSTEXPR const boost::string_view& as_string() const { return internal_as<boost::string_view>(); }
    BOOST_CXX14_CONSTEXPR const float& as_float() const { return internal_as<float>(); }
    BOOST_CXX14_CONSTEXPR const double& as_double() const { return internal_as<double>(); }
    BOOST_CXX14_CONSTEXPR const date& as_date() const { return internal_as<date>(); }
    BOOST_CXX14_CONSTEXPR const datetime& as_datetime() const { return internal_as<datetime>(); }
    BOOST_CXX14_CONSTEXPR const time& as_time() const { return internal_as<time>(); }

    BOOST_CXX14_CONSTEXPR const std::int64_t& get_int64() const noexcept { return internal_get<std::int64_t>(); }
    BOOST_CXX14_CONSTEXPR const std::uint64_t& get_uint64() const noexcept { return internal_get<std::uint64_t>(); }
    BOOST_CXX14_CONSTEXPR const boost::string_view& get_string() const noexcept { return internal_get<boost::string_view>(); }
    BOOST_CXX14_CONSTEXPR const float& get_float() const noexcept { return internal_get<float>(); }
    BOOST_CXX14_CONSTEXPR const double& get_double() const noexcept { return internal_get<double>(); }
    BOOST_CXX14_CONSTEXPR const date& get_date() const noexcept { return internal_get<date>(); }
    BOOST_CXX14_CONSTEXPR const datetime& get_datetime() const noexcept { return internal_get<datetime>(); }
    BOOST_CXX14_CONSTEXPR const time& get_time() const noexcept { return internal_get<time>(); }

    /// Tests for equality (type and value); see [link mysql.values.relational this section] for more info.
    BOOST_CXX14_CONSTEXPR bool operator==(const field_view& rhs) const noexcept;

    /// Tests for inequality (type and value); see [link mysql.values.relational this section] for more info.
    BOOST_CXX14_CONSTEXPR bool operator!=(const field_view& rhs) const noexcept { return !(*this==rhs); }

    // TODO: hide this
    void offset_to_string_view(const std::uint8_t* buffer_first) noexcept
    {
        auto* sv_index = boost::variant2::get_if<detail::string_view_offset>(&repr_);
        if (sv_index)
        {
            repr_ = sv_index->to_string_view(reinterpret_cast<const char*>(buffer_first));
        }
    }
private:
    struct print_visitor;
    
    using null_t = boost::variant2::monostate;

    using variant_type = boost::variant2::variant<
        null_t,            // Any of the below when the value is NULL
        std::int64_t,      // signed TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT
        std::uint64_t,     // unsigned TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT, YEAR, BIT
        boost::string_view,// CHAR, VARCHAR, BINARY, VARBINARY, TEXT (all sizes), BLOB (all sizes), ENUM, SET, DECIMAL, GEOMTRY
        float,             // FLOAT
        double,            // DOUBLE
        date,              // DATE
        datetime,          // DATETIME, TIMESTAMP
        time,              // TIME
        detail::string_view_offset // Used during parsing, not exposed to the user
    >;

    variant_type repr_;

    template <typename T>
    const T& internal_as() const;

    template <typename T>
    const T& internal_get() const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const field_view& v);
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
inline std::ostream& operator<<(std::ostream& os, const field_view& v);


/**
 * \relates value
 * \brief Creates an array of [reflink value]s out of the passed in arguments.
 * \details Each argument creates an element in the array. It should be possible
 * to construct a [reflink value] out of every single argument passed in.
 */
template <class... Types>
BOOST_CXX14_CONSTEXPR std::array<field_view, sizeof...(Types)> make_field_views(Types&&... args);

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

#include <boost/mysql/impl/field_view.hpp>


#endif
