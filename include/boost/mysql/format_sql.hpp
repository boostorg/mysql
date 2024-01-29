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
#include <type_traits>

#ifdef BOOST_MYSQL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace mysql {

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

class format_context_base
{
    struct
    {
        detail::output_string_ref output;
        format_options opts;
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
    format_context_base(detail::output_string_ref out, format_options opts, error_code ec = {}) noexcept
        : impl_{out, opts, ec}
    {
    }

    format_context_base(detail::output_string_ref out, const format_context_base& rhs) noexcept
        : impl_{out, rhs.impl_.opts, rhs.impl_.ec}
    {
    }

    void assign(const format_context_base& rhs) noexcept
    {
        // output never changes, it always points to the derived object's storage
        impl_.opts = rhs.impl_.opts;
        impl_.ec = rhs.impl_.ec;
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

template <class OutputString>
#ifdef BOOST_MYSQL_HAS_CONCEPTS
    requires detail::output_string<OutputString> && std::move_constructible<OutputString>
#endif
class basic_format_context : public format_context_base
{
    OutputString output_{};

    detail::output_string_ref ref() noexcept { return detail::output_string_ref::create(output_); }

public:
    basic_format_context(format_options opts
    ) noexcept(std::is_nothrow_default_constructible<OutputString>::value)
        : format_context_base(ref(), opts)
    {
    }

    basic_format_context(format_options opts, OutputString&& output) noexcept(
        std::is_nothrow_move_constructible<OutputString>::value
    )
        : format_context_base(ref(), opts), output_(std::move(output))
    {
    }

    basic_format_context(const basic_format_context&) = delete;
    basic_format_context& operator=(const basic_format_context&) = delete;

    basic_format_context(basic_format_context&& rhs
    ) noexcept(std::is_nothrow_move_constructible<OutputString>::value)
        : format_context_base(ref(), rhs), output_(std::move(rhs.output_))
    {
    }

    basic_format_context& operator=(basic_format_context&& rhs)
    {
        output_ = std::move(rhs.output_);
        assign(rhs);
    }

    // TODO: do we make this move-only?
    system::result<OutputString> get()
    {
        auto ec = get_error();
        if (ec)
            return ec;
        return std::move(output_);
    }
};

using format_context = basic_format_context<std::string>;

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
    format_context ctx(opts);
    detail::vformat_sql_to(format_str, ctx, desc);
    return ctx.get().value();
}

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/format_sql.ipp>
#endif

#endif
