//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_READABLE_FIELD_TRAITS_HPP
#define BOOST_MYSQL_DETAIL_TYPING_READABLE_FIELD_TRAITS_HPP

#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/non_null.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/time.hpp>

#include <boost/mysql/detail/auxiliar/is_optional.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/typing/meta_check_context.hpp>

#include <cstdint>
#include <limits>
#include <string>

namespace boost {
namespace mysql {
namespace detail {

template <class SignedInt, class UnsignedInt>
error_code parse_signed_int(field_view input, SignedInt& output)
{
    auto kind = input.kind();
    if (kind == field_kind::null)
    {
        return client_errc::is_null;
    }
    assert(kind == field_kind::int64 || kind == field_kind::uint64);
    if (kind == field_kind::int64)
    {
        auto v = input.get_int64();
        assert(v >= std::numeric_limits<SignedInt>::min());
        assert(v <= std::numeric_limits<SignedInt>::max());
        output = static_cast<SignedInt>(v);
    }
    else
    {
        auto v = input.get_uint64();
        assert(v <= static_cast<UnsignedInt>(std::numeric_limits<SignedInt>::max()));
        output = static_cast<SignedInt>(v);
    }
    return error_code();
}

template <class UnsignedInt>
error_code parse_unsigned_int(field_view input, UnsignedInt& output)
{
    auto kind = input.kind();
    if (kind == field_kind::null)
    {
        return client_errc::is_null;
    }
    assert(kind == field_kind::uint64);
    auto v = input.get_uint64();
    assert(v <= std::numeric_limits<UnsignedInt>::max());
    output = static_cast<UnsignedInt>(v);
    return error_code();
}

// Traits
template <typename T, bool is_optional_flag = is_optional<T>::value>
struct readable_field_traits
{
    static constexpr bool is_supported = false;
};

template <>
struct readable_field_traits<std::int8_t, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "int8_t";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::tinyint: return ctx.current_meta().is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::int8_t& output)
    {
        return parse_signed_int<std::int8_t, std::uint8_t>(input, output);
    }
};

template <>
struct readable_field_traits<std::uint8_t, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "uint8_t";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::tinyint: return ctx.current_meta().is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::uint8_t& output)
    {
        return parse_unsigned_int(input, output);
    }
};

template <>
struct readable_field_traits<bool, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "bool";
    static bool meta_check(meta_check_context& ctx)
    {
        return ctx.current_meta().type() == column_type::tinyint && !ctx.current_meta().is_unsigned();
    }
    static error_code parse(field_view input, bool& output)
    {
        assert(input.kind() == field_kind::int64);
        output = input.get_int64() != 0;
        return error_code();
    }
};

template <>
struct readable_field_traits<std::int16_t, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "int16_t";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::tinyint: return true;
        case column_type::smallint:
        case column_type::year: return !ctx.current_meta().is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::int16_t& output)
    {
        return parse_signed_int<std::int16_t, std::uint16_t>(input, output);
    }
};

template <>
struct readable_field_traits<std::uint16_t, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "uint16_t";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year: return ctx.current_meta().is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::uint16_t& output)
    {
        return parse_unsigned_int(input, output);
    }
};

template <>
struct readable_field_traits<std::int32_t, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "int32_t";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year:
        case column_type::mediumint: return true;
        case column_type::int_: return !ctx.current_meta().is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::int32_t& output)
    {
        return parse_signed_int<std::int32_t, std::uint32_t>(input, output);
    }
};

template <>
struct readable_field_traits<std::uint32_t, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "uint32_t";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year:
        case column_type::mediumint:
        case column_type::int_: return ctx.current_meta().is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::uint32_t& output)
    {
        return parse_unsigned_int(input, output);
    }
};

template <>
struct readable_field_traits<std::int64_t, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "int64_t";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year:
        case column_type::mediumint:
        case column_type::int_: return true;
        case column_type::bigint: return !ctx.current_meta().is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::int64_t& output)
    {
        return parse_signed_int<std::int64_t, std::uint64_t>(input, output);
    }
};

template <>
struct readable_field_traits<std::uint64_t, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "uint64_t";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year:
        case column_type::mediumint:
        case column_type::int_:
        case column_type::bigint: return ctx.current_meta().is_unsigned();
        case column_type::bit: return true;
        default: return false;
        }
    }
    static error_code parse(field_view input, std::uint64_t& output)
    {
        return parse_unsigned_int(input, output);
    }
};

template <>
struct readable_field_traits<float, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "float";
    static bool meta_check(meta_check_context& ctx)
    {
        return ctx.current_meta().type() == column_type::float_;
    }
    static error_code parse(field_view input, float& output)
    {
        auto kind = input.kind();
        if (kind == field_kind::null)
        {
            return client_errc::is_null;
        }
        assert(kind == field_kind::float_);
        output = input.get_float();
        return error_code();
    }
};

template <>
struct readable_field_traits<double, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "double";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::float_:
        case column_type::double_: return true;
        default: return false;
        }
    }
    static error_code parse(field_view input, double& output)
    {
        auto kind = input.kind();
        if (kind == field_kind::null)
        {
            return client_errc::is_null;
        }
        assert(kind == field_kind::float_ || kind == field_kind::double_);
        if (kind == field_kind::float_)
        {
            output = input.get_float();
        }
        else
        {
            output = input.get_double();
        }
        return error_code();
    }
};

template <class Traits, class Allocator>
struct readable_field_traits<std::basic_string<char, Traits, Allocator>, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "string";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::decimal:
        case column_type::char_:
        case column_type::varchar:
        case column_type::text:
        case column_type::enum_:
        case column_type::set:
        case column_type::json: return true;
        default: return false;
        }
    }
    static error_code parse(field_view input, std::basic_string<char, Traits, Allocator>& output)
    {
        auto kind = input.kind();
        if (kind == field_kind::null)
        {
            return client_errc::is_null;
        }
        assert(kind == field_kind::string);
        output = input.get_string();
        return error_code();
    }
};

template <class Allocator>
struct readable_field_traits<std::vector<unsigned char, Allocator>, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "blob";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::binary:
        case column_type::varbinary:
        case column_type::blob:
        case column_type::geometry:
        case column_type::unknown: return true;
        default: return false;
        }
    }
    static error_code parse(field_view input, std::vector<unsigned char, Allocator>& output)
    {
        auto kind = input.kind();
        if (kind == field_kind::null)
        {
            return client_errc::is_null;
        }
        assert(kind == field_kind::blob);
        auto view = input.get_blob();
        output.assign(view.begin(), view.end());
        return error_code();
    }
};

template <>
struct readable_field_traits<date, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "date";
    static bool meta_check(meta_check_context& ctx) { return ctx.current_meta().type() == column_type::date; }
    static error_code parse(field_view input, date& output)
    {
        auto kind = input.kind();
        if (kind == field_kind::null)
        {
            return client_errc::is_null;
        }
        assert(kind == field_kind::date);
        output = input.get_date();
        return error_code();
    }
};

template <>
struct readable_field_traits<datetime, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "datetime";
    static bool meta_check(meta_check_context& ctx)
    {
        switch (ctx.current_meta().type())
        {
        case column_type::datetime:
        case column_type::timestamp: return true;
        default: return false;
        }
    }
    static error_code parse(field_view input, datetime& output)
    {
        auto kind = input.kind();
        if (kind == field_kind::null)
        {
            return client_errc::is_null;
        }
        assert(kind == field_kind::datetime);
        output = input.get_datetime();
        return error_code();
    }
};

template <>
struct readable_field_traits<time, false>
{
    static constexpr bool is_supported = true;
    static constexpr const char* type_name = "time";
    static bool meta_check(meta_check_context& ctx) { return ctx.current_meta().type() == column_type::time; }
    static error_code parse(field_view input, time& output)
    {
        auto kind = input.kind();
        if (kind == field_kind::null)
        {
            return client_errc::is_null;
        }
        assert(kind == field_kind::time);
        output = input.get_time();
        return error_code();
    }
};

// std::optional<T> and boost::optional<T>. To avoid dependencies,
// this is achieved through a "concept"
template <class T>
struct readable_field_traits<T, true>
{
    using value_type = typename T::value_type;
    static constexpr bool is_supported = readable_field_traits<value_type>::is_supported;
    static constexpr const char* type_name = readable_field_traits<value_type>::type_name;
    static bool meta_check(meta_check_context& ctx)
    {
        ctx.set_nullability_checked();
        return readable_field_traits<value_type>::meta_check(ctx);
    }
    static error_code parse(field_view input, T& output)
    {
        if (input.is_null())
        {
            output.reset();
            return error_code();
        }
        else
        {
            output.emplace();
            return readable_field_traits<value_type>::parse(input, output.value());
        }
    }
};

template <class T>
struct readable_field_traits<non_null<T>, false>
{
    static constexpr bool is_supported = readable_field_traits<T>::is_supported;
    static constexpr const char* type_name = readable_field_traits<T>::type_name;
    static bool meta_check(meta_check_context& ctx)
    {
        ctx.set_nullability_checked();
        return readable_field_traits<T>::meta_check(ctx);
    }
    static error_code parse(field_view input, non_null<T>& output)
    {
        if (input.is_null())
        {
            return client_errc::is_null;
        }
        else
        {
            return readable_field_traits<T>::parse(input, output.value);
        }
    }
};

template <class T>
struct is_readable_field
{
    static constexpr bool value = readable_field_traits<T>::is_supported;
};

template <typename ReadableField>
void meta_check_field_impl(meta_check_context& ctx)
{
    using traits_t = readable_field_traits<ReadableField>;

    // Verify that the field is present
    if (ctx.is_current_field_absent())
    {
        ctx.add_field_absent_error();
        return;
    }

    // Perform the check
    bool ok = traits_t::meta_check(ctx);
    if (!ok)
    {
        ctx.add_type_mismatch_error(traits_t::type_name);
    }

    // Check nullability
    if (!ctx.nullability_checked() && !ctx.current_meta().is_not_null())
    {
        ctx.add_nullability_error();
    }
}

template <typename ReadableField>
void meta_check_field(meta_check_context& ctx)
{
    static_assert(is_readable_field<ReadableField>::value, "Should be a ReadableField");
    meta_check_field_impl<ReadableField>(ctx);
    ctx.advance();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
