//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FIELD_HPP
#define BOOST_MYSQL_FIELD_HPP

#include <boost/utility/string_view.hpp>
#include <boost/variant2/variant.hpp>
#include <boost/mysql/field_view.hpp>
#include <cstddef>
#include <ostream>
#include <string>


namespace boost {
namespace mysql {

class field
{
public:
    /// Constructs a NULL value.
    field() = default;
    field(const field&) = default;
    field(field&&) = default;
    field& operator=(const field&) = default;
    field& operator=(field&&) = default;
    ~field() = default;

    field(std::nullptr_t) noexcept : repr_(null_t()) {}
    field(null_t) noexcept : repr_(null_t()) {}
    field(signed char v) noexcept : repr_(std::int64_t(v)) {}
    field(short v) noexcept : repr_(std::int64_t(v)) {}
    field(int v) noexcept : repr_(std::int64_t(v)) {}
    field(long v) noexcept : repr_(std::int64_t(v)) {}
    field(long long v) noexcept : repr_(std::int64_t(v)) {}
    field(unsigned char v) noexcept : repr_(std::uint64_t(v)) {}
    field(unsigned short v) noexcept : repr_(std::uint64_t(v)) {}
    field(unsigned int v) noexcept : repr_(std::uint64_t(v)) {}
    field(unsigned long v) noexcept : repr_(std::uint64_t(v)) {}
    field(unsigned long long v) noexcept : repr_(std::uint64_t(v)) {}
    field(std::string&& v) noexcept : repr_(std::move(v)) {}
    field(float v) noexcept : repr_(v) {}
    field(double v) noexcept : repr_(v) {}
    field(const date& v) noexcept : repr_(v) {}
    field(const datetime& v) noexcept : repr_(v) {}
    field(const time& v) noexcept : repr_(v) {}
    field(const field_view& v) { from_view(v); }

    field& operator=(std::nullptr_t) noexcept { repr_.emplace<null_t>(null_t()); return *this; }
    field& operator=(null_t) noexcept { return (*this = nullptr); }
    field& operator=(signed char v) noexcept { repr_.emplace<std::int64_t>(v); return *this; }
    field& operator=(short v) noexcept { repr_.emplace<std::int64_t>(v); return *this; }
    field& operator=(int v) noexcept { repr_.emplace<std::int64_t>(v); return *this; }
    field& operator=(long v) noexcept { repr_.emplace<std::int64_t>(v); return *this; }
    field& operator=(long long v) noexcept { repr_.emplace<std::int64_t>(v); return *this; }
    field& operator=(unsigned char v) noexcept { repr_.emplace<std::uint64_t>(v); return *this; }
    field& operator=(unsigned short v) noexcept { repr_.emplace<std::uint64_t>(v); return *this; }
    field& operator=(unsigned int v) noexcept { repr_.emplace<std::uint64_t>(v); return *this; }
    field& operator=(unsigned long v) noexcept { repr_.emplace<std::uint64_t>(v); return *this; }
    field& operator=(unsigned long long v) noexcept { repr_.emplace<std::uint64_t>(v); return *this; }
    field& operator=(std::string&& v) { repr_.emplace<std::string>(std::move(v)); return *this; }
    field& operator=(float v) noexcept { repr_.emplace<float>(v); return *this; }
    field& operator=(double v) noexcept { repr_.emplace<double>(v); return *this; }
    field& operator=(const date& v) noexcept { repr_.emplace<date>(v); return *this; }
    field& operator=(const datetime& v) noexcept { repr_.emplace<datetime>(v); return *this; }
    field& operator=(const time& v) noexcept { repr_.emplace<time>(v); return *this; }
    field& operator=(const field_view& v) { from_view(v); return *this; }

    field_kind kind() const noexcept { return static_cast<field_kind>(repr_.index()); }

    bool is_null() const noexcept { return kind() == field_kind::null; }
    bool is_int64() const noexcept { return kind() == field_kind::int64; }
    bool is_uint64() const noexcept { return kind() == field_kind::uint64; }
    bool is_string() const noexcept { return kind() == field_kind::string; }
    bool is_float() const noexcept { return kind() == field_kind::float_; }
    bool is_double() const noexcept { return kind() == field_kind::double_; }
    bool is_date() const noexcept { return kind() == field_kind::date; }
    bool is_datetime() const noexcept { return kind() == field_kind::datetime; }
    bool is_time() const noexcept { return kind() == field_kind::time; }

    const std::int64_t* if_int64() const noexcept { return boost::variant2::get_if<std::int64_t>(&repr_); }
    const std::uint64_t* if_uint64() const noexcept { return boost::variant2::get_if<std::uint64_t>(&repr_); }
    const std::string* if_string() const noexcept { return boost::variant2::get_if<std::string>(&repr_); }
    const float* if_float() const noexcept { return boost::variant2::get_if<float>(&repr_); }
    const double* if_double() const noexcept { return boost::variant2::get_if<double>(&repr_); }
    const date* if_date() const noexcept { return boost::variant2::get_if<date>(&repr_); }
    const datetime* if_datetime() const noexcept { return boost::variant2::get_if<datetime>(&repr_); }
    const time* if_time() const noexcept { return boost::variant2::get_if<time>(&repr_); }

    std::int64_t* if_int64() noexcept { return boost::variant2::get_if<std::int64_t>(&repr_); }
    std::uint64_t* if_uint64() noexcept { return boost::variant2::get_if<std::uint64_t>(&repr_); }
    std::string* if_string() noexcept { return boost::variant2::get_if<std::string>(&repr_); }
    float* if_float() noexcept { return boost::variant2::get_if<float>(&repr_); }
    double* if_double() noexcept { return boost::variant2::get_if<double>(&repr_); }
    date* if_date() noexcept { return boost::variant2::get_if<date>(&repr_); }
    datetime* if_datetime() noexcept { return boost::variant2::get_if<datetime>(&repr_); }
    time* if_time() noexcept { return boost::variant2::get_if<time>(&repr_); }


    const std::int64_t& as_int64() const { return internal_as<std::int64_t>(); }
    const std::uint64_t& as_uint64() const { return internal_as<std::uint64_t>(); }
    const std::string& as_string() const { return internal_as<std::string>(); }
    const float& as_float() const { return internal_as<float>(); }
    const double& as_double() const { return internal_as<double>(); }
    const date& as_date() const { return internal_as<date>(); }
    const datetime& as_datetime() const { return internal_as<datetime>(); }
    const time& as_time() const { return internal_as<time>(); }

    std::int64_t& as_int64() { return internal_as<std::int64_t>(); }
    std::uint64_t& as_uint64() { return internal_as<std::uint64_t>(); }
    std::string& as_string() { return internal_as<std::string>(); }
    float& as_float() { return internal_as<float>(); }
    double& as_double() { return internal_as<double>(); }
    date& as_date() { return internal_as<date>(); }
    datetime& as_datetime() { return internal_as<datetime>(); }
    time& as_time() { return internal_as<time>(); }

    const std::int64_t& get_int64() const noexcept { return internal_get<std::int64_t>(); }
    const std::uint64_t& get_uint64() const noexcept { return internal_get<std::uint64_t>(); }
    const std::string& get_string() const noexcept { return internal_get<std::string>(); }
    const float& get_float() const noexcept { return internal_get<float>(); }
    const double& get_double() const noexcept { return internal_get<double>(); }
    const date& get_date() const noexcept { return internal_get<date>(); }
    const datetime& get_datetime() const noexcept { return internal_get<datetime>(); }
    const time& get_time() const noexcept { return internal_get<time>(); }

    std::int64_t& get_int64() noexcept { return internal_get<std::int64_t>(); }
    std::uint64_t& get_uint64() noexcept { return internal_get<std::uint64_t>(); }
    std::string& get_string() noexcept { return internal_get<std::string>(); }
    float& get_float() noexcept { return internal_get<float>(); }
    double& get_double() noexcept { return internal_get<double>(); }
    date& get_date() noexcept { return internal_get<date>(); }
    datetime& get_datetime() noexcept { return internal_get<datetime>(); }
    time& get_time() noexcept { return internal_get<time>(); }

    void emplace_null() noexcept { *this = null_t(); }
    std::int64_t& emplace_int64(std::int64_t v) noexcept { return (*this = v).get_int64(); }
    std::uint64_t& emplace_uint64(std::uint64_t v) noexcept { return (*this = v).get_uint64(); }
    std::string& emplace_string(std::string&& v) noexcept { return (*this = std::move(v)).get_string(); }
    float& emplace_float(float v) noexcept { return (*this = v).get_float(); }
    double& emplace_double(double v) noexcept { return (*this = v).get_double(); }
    date& emplace_date(const date& v) noexcept { return (*this = v).get_date(); }
    datetime& emplace_datetime(const datetime& v) noexcept { return (*this = v).get_datetime(); }
    time& emplace_time(const time& v) noexcept { return (*this = v).get_time(); }

    inline operator field_view() const noexcept;

    bool operator==(const field& rhs) const noexcept { return field_view(*this) == field_view(rhs); }
    bool operator!=(const field& rhs) const noexcept { return !(*this == rhs); }
private:
    using variant_type = boost::variant2::variant<
        null_t,            // Any of the below when the value is NULL
        std::int64_t,      // signed TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT
        std::uint64_t,     // unsigned TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT, YEAR, BIT
        std::string,       // CHAR, VARCHAR, BINARY, VARBINARY, TEXT (all sizes), BLOB (all sizes), ENUM, SET, DECIMAL, GEOMTRY
        float,             // FLOAT
        double,            // DOUBLE
        date,              // DATE
        datetime,          // DATETIME, TIMESTAMP
        time               // TIME
    >;

    variant_type repr_;

    template <typename T>
    const T& internal_as() const;

    template <typename T>
    T& internal_as();

    template <typename T>
    const T& internal_get() const noexcept;

    template <typename T>
    T& internal_get() noexcept;

    inline void from_view(const field_view& v);
};

inline std::ostream& operator<<(std::ostream& os, const field& v);

} // mysql
} // boost

#include <boost/mysql/impl/field.hpp>


#endif
