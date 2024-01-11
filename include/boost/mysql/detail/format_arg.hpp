//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_FORMAT_ARG_HPP
#define BOOST_MYSQL_DETAIL_FORMAT_ARG_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/writable_field_traits.hpp>

namespace boost {
namespace mysql {

// Forward decls
template <class T>
struct formatter;

class format_context;

namespace detail {

class format_state;

// A type-erased custom argument passed to format, invoking formatter<T>::format
struct format_custom_arg
{
    const void* obj;
    void (*format_fn)(const void*, format_context&);

    template <class T>
    static format_custom_arg create(const T& obj) noexcept
    {
        return {&obj, &do_format<T>};
    }

    template <class T>
    static void do_format(const void* obj, format_context& ctx)
    {
        formatter<T>::format(*static_cast<const T*>(obj), ctx);
    }
};

// A type-erased argument passed to format. Built-in types are passed
// directly in the struct (as a field_view), instead of by pointer,
// to reduce the number of do_format instantiations
struct format_arg_value
{
    bool is_custom;
    union data_t
    {
        field_view fv;
        format_custom_arg custom;

        data_t(field_view fv) noexcept : fv(fv) {}
        data_t(format_custom_arg v) noexcept : custom(v) {}
    } data;
};

// make_format_value: creates a type erased format_arg_value from a typed value.
// Used for types having is_writable_field<T>
template <class T>
format_arg_value make_format_value_impl(const T& v, std::true_type) noexcept
{
    return {false, to_field(v)};
}

// Used for types having !is_writable_field<T>
template <class T>
format_arg_value make_format_value_impl(const T& v, std::false_type) noexcept
{
    return {true, format_custom_arg::create(v)};
}

template <class T>
format_arg_value make_format_value(const T& v) noexcept
{
    return make_format_value_impl(v, is_writable_field<T>{});
}

// A (name, value) pair
// TODO: can we split names and values? should hit cache less
struct format_arg_descriptor
{
    format_arg_value value;
    string_view name;
};

// Pass-through anything that is already a format_arg_descriptor.
// Used by named arguments
inline format_arg_descriptor make_format_arg_descriptor(const format_arg_descriptor& v) noexcept { return v; }

template <class T>
format_arg_descriptor make_format_arg_descriptor(const T& val)
{
    return {make_format_value(val), {}};
}

BOOST_MYSQL_DECL
void vformat_to(
    string_view format_str,
    const format_context& ctx,
    span<const detail::format_arg_descriptor> args
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
