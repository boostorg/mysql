//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FORMAT_HPP
#define BOOST_MYSQL_FORMAT_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/format_arg.hpp>
#include <boost/mysql/detail/output_string_ref.hpp>

#include <boost/config.hpp>
#include <boost/core/span.hpp>

#include <array>
#include <string>

namespace boost {
namespace mysql {

// public
class raw_sql
{
    string_view impl_;

public:
    BOOST_CXX14_CONSTEXPR raw_sql() = default;
    BOOST_CXX14_CONSTEXPR explicit raw_sql(string_view v) noexcept : impl_(v) {}
    BOOST_CXX14_CONSTEXPR string_view get() const noexcept { return impl_; }
};

class identifier
{
    string_view id1, id2, id3;

public:
    BOOST_CXX14_CONSTEXPR explicit identifier(
        string_view id1,
        string_view id2 = {},
        string_view id3 = {}
    ) noexcept
        : id1(id1), id2(id2), id3(id3)
    {
    }
    BOOST_CXX14_CONSTEXPR string_view first() const noexcept { return id1; }
    BOOST_CXX14_CONSTEXPR string_view second() const noexcept { return id2; }
    BOOST_CXX14_CONSTEXPR string_view third() const noexcept { return id3; }
};

template <class T>
struct formatter;

struct format_options
{
    character_set charset;
    bool backslash_escapes;
};

// detail

// a big part of this will be in ipp
class format_context
{
    struct
    {
        detail::output_string_ref output;
        const format_options opts;
    } impl_;

    friend struct detail::access;
    friend class detail::format_state;

    BOOST_MYSQL_DECL
    void format_arg(detail::format_arg_value arg);

public:
    format_context(detail::output_string_ref out, format_options opts) noexcept : impl_{out, opts} {}

    void append_raw(string_view raw_sql) { impl_.output.append(raw_sql); }

    template <class T>
    void append_value(const T& v)
    {
        format_arg(create_arg_value(v));
    }
};

template <>
struct formatter<raw_sql>
{
    static void format(const raw_sql& value, format_context& ctx) { ctx.append_raw(value.get()); }
};

template <>
struct formatter<identifier>
{
    BOOST_MYSQL_DECL
    static void format(const identifier& value, format_context& ctx);
};

// TODO: hide this
BOOST_MYSQL_DECL
void vformat_to(
    string_view format_str,
    detail::output_string_ref output,
    const format_options& opts,
    span<const detail::format_arg_descriptor> args
);

template <class T>
inline detail::format_arg_descriptor arg(const T& value, string_view name) noexcept
{
    return {detail::make_format_value(value), name};
}

template <BOOST_MYSQL_OUTPUT_STRING Output, class... Args>
inline void format_to(string_view format_str, Output& output, const format_options& opts, const Args&... args)
{
    std::array<detail::format_arg_descriptor, sizeof...(Args)> desc{
        {detail::make_format_arg_descriptor(args...)}
    };
    vformat_to(format_str, detail::output_string_ref::create(output), opts, desc);
}

template <class... Args>
inline std::string format(string_view format_str, const format_options& opts, const Args&... args)
{
    std::string output;
    format_to(format_str, output, opts, args...);
    return output;
}

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/format.ipp>
#endif

#endif
