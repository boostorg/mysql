//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FIELD_HPP
#define BOOST_MYSQL_FIELD_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/datetime_types.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/detail/auxiliar/field_impl.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/variant2/variant.hpp>
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

    explicit field(std::nullptr_t) noexcept {}
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
    field(std::string v) noexcept : repr_(std::move(v)) {}
    field(float v) noexcept : repr_(v) {}
    field(double v) noexcept : repr_(v) {}
    field(const date& v) noexcept : repr_(v) {}
    field(const datetime& v) noexcept : repr_(v) {}
    field(const time& v) noexcept : repr_(v) {}
    field(const field_view& v) { from_view(v); }

    field& operator=(std::nullptr_t) noexcept { repr_.data.emplace<detail::field_impl::null_t>(); return *this; }
    field& operator=(signed char v) noexcept { repr_.data.emplace<std::int64_t>(v); return *this; }
    field& operator=(short v) noexcept { repr_.data.emplace<std::int64_t>(v); return *this; }
    field& operator=(int v) noexcept { repr_.data.emplace<std::int64_t>(v); return *this; }
    field& operator=(long v) noexcept { repr_.data.emplace<std::int64_t>(v); return *this; }
    field& operator=(long long v) noexcept { repr_.data.emplace<std::int64_t>(v); return *this; }
    field& operator=(unsigned char v) noexcept { repr_.data.emplace<std::uint64_t>(v); return *this; }
    field& operator=(unsigned short v) noexcept { repr_.data.emplace<std::uint64_t>(v); return *this; }
    field& operator=(unsigned int v) noexcept { repr_.data.emplace<std::uint64_t>(v); return *this; }
    field& operator=(unsigned long v) noexcept { repr_.data.emplace<std::uint64_t>(v); return *this; }
    field& operator=(unsigned long long v) noexcept { repr_.data.emplace<std::uint64_t>(v); return *this; }
    field& operator=(std::string v) { repr_.data.emplace<std::string>(std::move(v)); return *this; }
    field& operator=(float v) noexcept { repr_.data.emplace<float>(v); return *this; }
    field& operator=(double v) noexcept { repr_.data.emplace<double>(v); return *this; }
    field& operator=(const date& v) noexcept { repr_.data.emplace<date>(v); return *this; }
    field& operator=(const datetime& v) noexcept { repr_.data.emplace<datetime>(v); return *this; }
    field& operator=(const time& v) noexcept { repr_.data.emplace<time>(v); return *this; }
    field& operator=(const field_view& v) { from_view(v); return *this; }

    field_kind kind() const noexcept { return repr_.kind(); }

    bool is_null() const noexcept { return kind() == field_kind::null; }
    bool is_int64() const noexcept { return kind() == field_kind::int64; }
    bool is_uint64() const noexcept { return kind() == field_kind::uint64; }
    bool is_string() const noexcept { return kind() == field_kind::string; }
    bool is_float() const noexcept { return kind() == field_kind::float_; }
    bool is_double() const noexcept { return kind() == field_kind::double_; }
    bool is_date() const noexcept { return kind() == field_kind::date; }
    bool is_datetime() const noexcept { return kind() == field_kind::datetime; }
    bool is_time() const noexcept { return kind() == field_kind::time; }

    const std::int64_t& as_int64() const { return repr_.as<std::int64_t>(); }
    const std::uint64_t& as_uint64() const { return repr_.as<std::uint64_t>(); }
    const std::string& as_string() const { return repr_.as<std::string>(); }
    const float& as_float() const { return repr_.as<float>(); }
    const double& as_double() const { return repr_.as<double>(); }
    const date& as_date() const { return repr_.as<date>(); }
    const datetime& as_datetime() const { return repr_.as<datetime>(); }
    const time& as_time() const { return repr_.as<time>(); }

    std::int64_t& as_int64() { return repr_.as<std::int64_t>(); }
    std::uint64_t& as_uint64() { return repr_.as<std::uint64_t>(); }
    std::string& as_string() { return repr_.as<std::string>(); }
    float& as_float() { return repr_.as<float>(); }
    double& as_double() { return repr_.as<double>(); }
    date& as_date() { return repr_.as<date>(); }
    datetime& as_datetime() { return repr_.as<datetime>(); }
    time& as_time() { return repr_.as<time>(); }

    const std::int64_t& get_int64() const noexcept { return repr_.get<std::int64_t>(); }
    const std::uint64_t& get_uint64() const noexcept { return repr_.get<std::uint64_t>(); }
    const std::string& get_string() const noexcept { return repr_.get<std::string>(); }
    const float& get_float() const noexcept { return repr_.get<float>(); }
    const double& get_double() const noexcept { return repr_.get<double>(); }
    const date& get_date() const noexcept { return repr_.get<date>(); }
    const datetime& get_datetime() const noexcept { return repr_.get<datetime>(); }
    const time& get_time() const noexcept { return repr_.get<time>(); }

    std::int64_t& get_int64() noexcept { return repr_.get<std::int64_t>(); }
    std::uint64_t& get_uint64() noexcept { return repr_.get<std::uint64_t>(); }
    std::string& get_string() noexcept { return repr_.get<std::string>(); }
    float& get_float() noexcept { return repr_.get<float>(); }
    double& get_double() noexcept { return repr_.get<double>(); }
    date& get_date() noexcept { return repr_.get<date>(); }
    datetime& get_datetime() noexcept { return repr_.get<datetime>(); }
    time& get_time() noexcept { return repr_.get<time>(); }

    void emplace_null() noexcept { *this = nullptr; }
    void emplace_int64(std::int64_t v) noexcept { *this = v; }
    void emplace_uint64(std::uint64_t v) noexcept { *this = v; }
    void emplace_string(std::string v) noexcept { *this = std::move(v); }
    void emplace_float(float v) noexcept { *this = v; }
    void emplace_double(double v) noexcept { *this = v; }
    void emplace_date(const date& v) noexcept { *this = v; }
    void emplace_datetime(const datetime& v) noexcept { *this = v; }
    void emplace_time(const time& v) noexcept { *this = v; }

    inline operator field_view() const noexcept { return field_view(&repr_); }

    bool operator==(const field& rhs) const noexcept { return field_view(*this) == field_view(rhs); }
    bool operator!=(const field& rhs) const noexcept { return !(*this == rhs); }
private:
    detail::field_impl repr_;

    inline void from_view(const field_view& v);
};

inline std::ostream& operator<<(std::ostream& os, const field& v);

} // mysql
} // boost

#include <boost/mysql/impl/field.hpp>


#endif
