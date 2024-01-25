//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FORMAT_SQL_HPP
#define BOOST_MYSQL_FORMAT_SQL_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/format_sql.hpp>
#include <boost/mysql/detail/output_string_ref.hpp>

#include <boost/config.hpp>
#include <boost/core/span.hpp>
#include <boost/system/result.hpp>

#include <array>
#include <string>

namespace boost {
namespace mysql {

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
struct formatter : detail::formatter_is_unspecialized
{
};

struct format_options
{
    character_set charset;
    bool backslash_escapes;
};

class format_context_base
{
    struct
    {
        detail::output_string_ref output;
        const format_options opts;
        error_code ec;

        void set_error(error_code new_ec) noexcept
        {
            if (!ec)
                ec = new_ec;
        }
    } impl_;

    friend struct detail::access;
    friend class detail::format_state;

    BOOST_MYSQL_DECL void format_arg(detail::format_arg_value arg);

protected:
    format_context_base(detail::output_string_ref out, format_options opts) noexcept
        : impl_{out, opts, error_code()}
    {
    }

    error_code get_error() const noexcept { return impl_.ec; }

public:
    format_context_base& append_raw(string_view raw_sql)
    {
        impl_.output.append(raw_sql);
        return *this;
    }

    template <BOOST_MYSQL_FORMATTABLE T>
    format_context_base& append_value(const T& v)
    {
        format_arg(detail::make_format_value(v));
        return *this;
    }
};

template <BOOST_MYSQL_OUTPUT_STRING OutputString>
class basic_format_context : public format_context_base
{
    OutputString output_{};

public:
    // TODO: noexcept specifier
    // TODO: this requires OutputString to also be MoveConstructible, and possibly DefaultConstructible
    basic_format_context(format_options opts)
        : format_context_base(detail::output_string_ref::create(output_), opts)
    {
    }

    basic_format_context(format_options opts, OutputString&& output)
        : basic_format_context(opts), output_(std::move(output))
    {
    }

    // TODO: this can be implemented
    basic_format_context(const basic_format_context&) = delete;
    basic_format_context(basic_format_context&&) = delete;
    basic_format_context& operator=(const basic_format_context&) = delete;
    basic_format_context& operator=(basic_format_context&&) = delete;

    system::result<OutputString> get()
    {
        auto ec = get_error();
        if (ec)
            return ec;
        return std::move(output_);
    }
};

template <>
struct formatter<raw_sql>
{
    static void format(const raw_sql& value, format_context_base& ctx) { ctx.append_raw(value.get()); }
};

template <>
struct formatter<identifier>
{
    BOOST_MYSQL_DECL
    static void format(const identifier& value, format_context_base& ctx);
};

template <class T>
inline detail::format_arg_descriptor arg(string_view name, const T& value) noexcept
{
    return {detail::make_format_value(value), name};
}

template <BOOST_MYSQL_FORMATTABLE... Args>
inline std::string format_sql(string_view format_str, const format_options& opts, const Args&... args)
{
    std::array<detail::format_arg_descriptor, sizeof...(Args)> desc{{detail::make_format_arg_descriptor(args
    )...}};
    basic_format_context<std::string> ctx(opts);
    detail::vformat_sql_to(format_str, ctx, desc);
    return ctx.get().value();
}

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/format_sql.ipp>
#endif

#endif
