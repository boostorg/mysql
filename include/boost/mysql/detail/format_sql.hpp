//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_FORMAT_SQL_HPP
#define BOOST_MYSQL_DETAIL_FORMAT_SQL_HPP

#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/writable_field_traits.hpp>

#include <initializer_list>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

#ifdef BOOST_MYSQL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace mysql {

// Forward decls
template <class T>
struct formatter;

class format_context_base;
class format_arg;
struct format_options;

namespace detail {

class format_state;

struct formatter_is_unspecialized
{
};

template <class T>
constexpr bool has_specialized_formatter()
{
    return !std::is_base_of<formatter_is_unspecialized, formatter<typename std::decay<T>::type>>::value;
}

template <class T>
struct is_writable_field_ref : is_writable_field<typename std::decay<T>::type>
{
};

template <class T>
constexpr bool is_writable_or_custom_formattable()
{
    return is_writable_field_ref<T>::value || has_specialized_formatter<T>();
}

template <class T, class = void>
struct is_formattable_range : std::false_type
{
};

template <class T>
struct is_formattable_range<
    T,
    typename std::enable_if<
        // std::begin and std::end can be called on it, and we can compare values
        std::is_convertible<decltype(std::begin(std::declval<T>()) != std::end(std::declval<T>())), bool>::
            value &&

        // value_type is either a writable field or a type with a specialized formatter.
        // We don't support sequences of sequences out of the box (no known use case)
        is_writable_or_custom_formattable<decltype(*std::begin(std::declval<T>()))>()

        // end of conditions
        >::type> : std::true_type
{
};

template <class T>
constexpr bool is_formattable_type()
{
    return is_writable_or_custom_formattable<T>() || is_formattable_range<T>::value;
}

#ifdef BOOST_MYSQL_HAS_CONCEPTS

// If you're getting an error referencing this concept,
// it means that you are attempting to format a type that doesn't support it.
template <class T>
concept formattable =
    // This covers basic types and optionals
    is_writable_field_ref<T>::value ||
    // This covers custom types that specialized boost::mysql::formatter
    has_specialized_formatter<T>() ||
    // This covers ranges of formattable types
    is_formattable_range<T>::value;

template <class FormatFn, class Range>
concept format_fn_for_range = requires(const FormatFn& format_fn, Range&& range, format_context_base& ctx) {
    { std::begin(range) != std::end(range) } -> std::convertible_to<bool>;
    format_fn(*std::begin(range), ctx);
    std::end(range);
};

#define BOOST_MYSQL_FORMATTABLE ::boost::mysql::detail::formattable

#else

#define BOOST_MYSQL_FORMATTABLE class

#endif

// A type-erased custom argument passed to format
struct format_custom_arg
{
    const void* obj;
    bool (*format_fn)(const void*, const char*, const char*, format_context_base&);

    // To use with arguments with a custom formatter
    template <class T>
    static bool do_format(
        const void* obj,
        const char* spec_begin,
        const char* spec_end,
        format_context_base& ctx
    )
    {
        formatter<T> fmt;
        const char* it = fmt.parse(spec_begin, spec_end);
        if (it != spec_end)
        {
            return false;
        }
        fmt.format(*static_cast<const T*>(obj), ctx);
        return true;
    }

    template <class T>
    static format_custom_arg create_custom_formatter(const T& obj) noexcept
    {
        return {&obj, &do_format<T>};
    }

    // To use with ranges
    // Definition depends on format_context_base
    template <class T>
    static bool do_format_range(
        const void* obj,
        const char* spec_begin,
        const char* spec_end,
        format_context_base& ctx
    );

    template <class T>
    static format_custom_arg create_range(const T& obj)
    {
        return {&obj, &do_format_range<const T>};
    }

    template <class T>
    static format_custom_arg create_range(T& obj)
    {
        return {&obj, &do_format_range<T>};
    }
};

// A type-erased argument passed to format. Built-in types are passed
// directly in the struct (as a field_view), instead of by pointer,
// to reduce the number of do_format instantiations
struct format_arg_value
{
    enum class type_t
    {
        string,
        field,
        custom
    };

    union data_t
    {
        string_view s;
        field_view fv;
        format_custom_arg custom;

        data_t(string_view v) noexcept : s(v) {}
        data_t(field_view fv) noexcept : fv(fv) {}
        data_t(format_custom_arg v) noexcept : custom(v) {}
    };

    type_t type;
    data_t data;
};

// make_format_value: creates a type erased format_arg_value from a typed value.
// Used for types convertible to string view. We must differentiate this from
// field_views and optionals because supported specifiers are different
template <class T, bool is_rng>
format_arg_value make_format_value_impl(
    const T& v,
    std::true_type,  // convertible to string view
    std::true_type,  // if it's convertible to string_view, it will be a writable field, too
    std::integral_constant<bool, is_rng>  // is formattable range: we don't care
) noexcept
{
    return {format_arg_value::type_t::string, string_view(v)};
}

// Used for types having is_writable_field<T>
template <class T, bool is_rng>
format_arg_value make_format_value_impl(
    const T& v,
    std::false_type,                      // convertible to string view
    std::true_type,                       // is_writable_field
    std::integral_constant<bool, is_rng>  // is formattable range: we don't care
) noexcept
{
    return {format_arg_value::type_t::field, to_field(v)};
}

// Used for types having is_formattable_range
template <class T>
format_arg_value make_format_value_impl(
    T&& v,
    std::false_type,  // convertible to string view
    std::false_type,  // is_writable_field
    std::true_type    // is formattable range
) noexcept
{
    return {format_arg_value::type_t::custom, format_custom_arg::create_range(v)};
}

// Used for types having !is_writable_field<T>
template <class T>
format_arg_value make_format_value_impl(
    const T& v,
    std::false_type,  // convertible to string view
    std::false_type,  // writable field
    std::false_type   // is formattable range
) noexcept
{
    return {format_arg_value::type_t::custom, format_custom_arg::create_custom_formatter(v)};
}

template <class T>
format_arg_value make_format_value(T&& v) noexcept
{
    static_assert(
        is_formattable_type<T>(),
        "T is not formattable. Please use a formattable type or specialize formatter<T> to make it "
        "formattable"
    );
    return make_format_value_impl(
        std::forward<T>(v),
        std::is_convertible<T, string_view>(),
        is_writable_field_ref<T>(),
        is_formattable_range<T>()
    );
}

BOOST_MYSQL_DECL
void vformat_sql_to(format_context_base& ctx, string_view format_str, std::initializer_list<format_arg> args);

BOOST_MYSQL_DECL
std::string vformat_sql(
    const format_options& opts,
    string_view format_str,
    std::initializer_list<format_arg> args
);

BOOST_MYSQL_DECL
std::pair<bool, string_view> parse_range_specifiers(const char* spec_begin, const char* spec_end);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
