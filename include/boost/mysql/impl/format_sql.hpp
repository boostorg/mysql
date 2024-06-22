//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_FORMAT_SQL_HPP
#define BOOST_MYSQL_IMPL_FORMAT_SQL_HPP

#pragma once

#include <boost/mysql/format_sql.hpp>

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

BOOST_MYSQL_DECL
std::pair<bool, string_view> parse_range_specifiers(const char* spec_begin, const char* spec_end);

// To use with arguments with a custom formatter
template <class T>
bool do_format_custom_formatter(
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

// To use with ranges
template <class T>
bool do_format_range(const void* obj, const char* spec_begin, const char* spec_end, format_context_base& ctx)
{
    // Parse specifiers
    auto res = detail::parse_range_specifiers(spec_begin, spec_end);
    if (!res.first)
        return false;
    auto spec = runtime(res.second);

    // Retrieve the object. T here may be the actual type U or const U
    auto& value = *const_cast<T*>(static_cast<const T*>(obj));

    // Output the sequence
    bool is_first = true;
    for (auto it = std::begin(value); it != std::end(value); ++it)
    {
        if (!is_first)
            ctx.append_raw(", ");
        is_first = false;
        ctx.append_value(*it, spec);
    }
    return true;
}

// make_format_value: creates a type erased format_arg_value from a typed value.
// Used for types having is_writable_field<T>
template <class T, bool is_rng>
formattable_ref_impl make_format_value_impl(
    const T& v,
    std::true_type,                        // is_writable_field
    std::integral_constant<bool, is_rng>,  // is formattable range: we don't care
    std::false_type                        // is formattable ref
)
{
    // Only string types (and not field_views or optionals) support the string specifiers
    return {
        std::is_convertible<T, string_view>::value ? formattable_ref_impl::type_t::field_with_specs
                                                   : formattable_ref_impl::type_t::field,
        to_field(v)
    };
}

// Used for types having is_formattable_range
template <class T>
formattable_ref_impl make_format_value_impl(
    T&& v,
    std::false_type,  // writable field
    std::true_type,   // is formattable range
    std::false_type   // is formattable ref
)
{
    // Although everything is passed as const void*, do_format_range
    // can bypass const-ness for non-const ranges (e.g. filter_view)
    return {
        formattable_ref_impl::type_t::fn_and_ptr,
        formattable_ref_impl::fn_and_ptr{&v, &do_format_range<typename std::remove_reference<T>::type>}
    };
}

// Used for types having !is_writable_field<T>
template <class T>
formattable_ref_impl make_format_value_impl(
    const T& v,
    std::false_type,  // writable field
    std::false_type,  // is formattable range
    std::false_type   // is formattable ref
)
{
    return {
        formattable_ref_impl::type_t::fn_and_ptr,
        formattable_ref_impl::fn_and_ptr{&v, &do_format_custom_formatter<T>}
    };
}

// Used for formattable_ref
// TODO: restructure
inline formattable_ref_impl make_format_value_impl(
    formattable_ref v,
    std::false_type,  // writable field
    std::false_type,  // is formattable range
    std::true_type    // is formattable ref
)
{
    return access::get_impl(v);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class T>
boost::mysql::detail::formattable_ref_impl boost::mysql::detail::make_formattable_ref(T&& v)
{
    static_assert(
        is_formattable_type<T>(),
        "T is not formattable. Please use a formattable type or specialize formatter<T> to make it "
        "formattable"
    );
    return make_format_value_impl(
        std::forward<T>(v),
        is_writable_field_ref<T>(),
        is_formattable_range<T>(),
        is_formattable_ref<T>()
    );
}

template <BOOST_MYSQL_FORMATTABLE... Formattable>
void boost::mysql::format_sql_to(
    format_context_base& ctx,
    constant_string_view format_str,
    Formattable&&... args
)
{
    std::initializer_list<format_arg> args_il{
        {string_view(), std::forward<Formattable>(args)}
        ...
    };
    format_sql_to(ctx, format_str, args_il);
}

template <BOOST_MYSQL_FORMATTABLE... Formattable>
std::string boost::mysql::format_sql(
    format_options opts,
    constant_string_view format_str,
    Formattable&&... args
)
{
    std::initializer_list<format_arg> args_il{
        {string_view(), std::forward<Formattable>(args)}
        ...
    };
    return format_sql(opts, format_str, args_il);
}

#endif
