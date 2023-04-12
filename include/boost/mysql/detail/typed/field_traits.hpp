//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPED_FIELD_TRAITS_HPP
#define BOOST_MYSQL_DETAIL_TYPED_FIELD_TRAITS_HPP

#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/time.hpp>

#include <boost/mysql/detail/typed/meta_check_context.hpp>

#include <boost/config.hpp>
#include <boost/optional/optional.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif

namespace boost {
namespace mysql {
namespace detail {

// Helpers
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
        assert(v <= std::numeric_limits<SignedInt>::max());
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

inline void add_on_error(meta_check_context& ctx, bool ok)
{
    if (!ok)
        ctx.add_error("types are incompatible");
}

struct valid_field_traits
{
    static constexpr bool is_supported = true;
};

// Traits
template <typename T>
struct field_traits
{
    static constexpr bool is_supported = false;
};

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
template <class T>
struct field_traits<std::optional<T>>
{
    static constexpr bool is_supported = field_traits<T>::is_supported;
    static constexpr const char* type_name = "std::optional<T>";
    static void meta_check(meta_check_context& ctx)
    {
        ctx.set_cpp_type_name(field_traits<T>::type_name);
        field_traits<T>::meta_check(ctx);
    }
    static error_code parse(field_view input, std::optional<T>& output)
    {
        if (input.is_null())
        {
            output = std::optional<T>{};
            return error_code();
        }
        else
        {
            return field_traits<T>::parse(input, output.emplace());
        }
    }
};
#endif

template <class T>
struct field_traits<boost::optional<T>>
{
    static constexpr bool is_supported = field_traits<T>::is_supported;
    static constexpr const char* type_name = "boost::optional<T>";
    static void meta_check(meta_check_context& ctx)
    {
        ctx.set_cpp_type_name(field_traits<T>::type_name);
        field_traits<T>::meta_check(ctx);
    }
    static error_code parse(field_view input, boost::optional<T>& output)
    {
        if (input.is_null())
        {
            output = boost::optional<T>{};
            return error_code();
        }
        else
        {
            output.emplace();
            return field_traits<T>::parse(input, *output);
        }
    }
};

template <>
struct field_traits<std::int8_t> : valid_field_traits
{
    static constexpr const char* type_name = "int8_t";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
        {
        case column_type::tinyint: return meta.is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::int8_t& output)
    {
        return parse_signed_int<std::int8_t, std::uint8_t>(input, output);
    }
};

template <>
struct field_traits<std::uint8_t> : valid_field_traits
{
    static constexpr const char* type_name = "uint8_t";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
        {
        case column_type::tinyint: return meta.is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::uint8_t& output)
    {
        return parse_unsigned_int(input, output);
    }
};

template <>
struct field_traits<std::int16_t> : valid_field_traits
{
    static constexpr const char* type_name = "int16_t";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
        {
        case column_type::tinyint: return true;
        case column_type::smallint:
        case column_type::year: return !meta.is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::int16_t& output)
    {
        return parse_signed_int<std::int16_t, std::uint16_t>(input, output);
    }
};

template <>
struct field_traits<std::uint16_t> : valid_field_traits
{
    static constexpr const char* type_name = "uint16_t";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year: return meta.is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::uint16_t& output)
    {
        return parse_unsigned_int(input, output);
    }
};

template <>
struct field_traits<std::int32_t> : valid_field_traits
{
    static constexpr const char* type_name = "int32_t";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year:
        case column_type::mediumint: return true;
        case column_type::int_: return !meta.is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::int32_t& output)
    {
        return parse_signed_int<std::int32_t, std::uint32_t>(input, output);
    }
};

template <>
struct field_traits<std::uint32_t> : valid_field_traits
{
    static constexpr const char* type_name = "uint32_t";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year:
        case column_type::mediumint:
        case column_type::int_: return meta.is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::uint32_t& output)
    {
        return parse_unsigned_int(input, output);
    }
};

template <>
struct field_traits<std::int64_t> : valid_field_traits
{
    static constexpr const char* type_name = "int64_t";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year:
        case column_type::mediumint:
        case column_type::int_: return true;
        case column_type::bigint: return !meta.is_unsigned();
        default: return false;
        }
    }
    static error_code parse(field_view input, std::int64_t& output)
    {
        return parse_signed_int<std::int64_t, std::uint64_t>(input, output);
    }
};

template <>
struct field_traits<std::uint64_t> : valid_field_traits
{
    static constexpr const char* type_name = "uint64_t";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
        {
        case column_type::tinyint:
        case column_type::smallint:
        case column_type::year:
        case column_type::mediumint:
        case column_type::int_:
        case column_type::bigint: return meta.is_unsigned();
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
struct field_traits<float> : valid_field_traits
{
    static constexpr const char* type_name = "float";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta) { return meta.type() == column_type::float_; }
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
struct field_traits<double> : valid_field_traits
{
    static constexpr const char* type_name = "double";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
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
struct field_traits<std::basic_string<char, Traits, Allocator>> : valid_field_traits
{
    static constexpr const char* type_name = "string";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
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
struct field_traits<std::vector<unsigned char, Allocator>> : valid_field_traits
{
    static constexpr const char* type_name = "blob";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
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
struct field_traits<date> : valid_field_traits
{
    static constexpr const char* type_name = "date";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta) { return meta.type() == column_type::date; }
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
struct field_traits<datetime> : valid_field_traits
{
    static constexpr const char* type_name = "datetime";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta)
    {
        switch (meta.type())
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
struct field_traits<time> : valid_field_traits
{
    static constexpr const char* type_name = "time";
    static void meta_check(meta_check_context& ctx)
    {
        add_on_error(ctx, meta_check_impl(ctx.current_meta()));
    }
    static bool meta_check_impl(const metadata& meta) { return meta.type() == column_type::time; }
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

template <typename FieldType>
void meta_check_impl(meta_check_context& ctx)
{
    using traits_t = field_traits<FieldType>;
    ctx.set_cpp_type_name(traits_t::type_name);
    traits_t::meta_check(ctx);
    ctx.advance();
}

template <class T>
struct is_field_type
{
    static constexpr bool value = field_traits<T>::is_supported;
};

#ifdef BOOST_MYSQL_HAS_CONCEPTS

template <class T>
concept field_type = is_field_type<T>::value;

#define BOOST_MYSQL_FIELD_TYPE ::boost::mysql::detail::field_type

#else
#define BOOST_MYSQL_FIELD_TYPE class
#endif

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
