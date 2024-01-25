//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_FORMAT_SQL_IPP
#define BOOST_MYSQL_IMPL_FORMAT_SQL_IPP

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/escape_string.hpp>
#include <boost/mysql/detail/format_sql.hpp>
#include <boost/mysql/detail/output_string_ref.hpp>

#include <boost/mysql/impl/internal/dt_to_string.hpp>

#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

// Helpers to format fundamental types
inline error_code append_identifier(string_view name, format_context& ctx)
{
    auto& impl = access::get_impl(ctx);
    return detail::escape_string(name, impl.opts.charset, impl.opts.backslash_escapes, '`', impl.output);
}

template <class T>
void append_int(output_string_ref output, T integer)
{
    // Make sure our buffer is big enough. 2: sign + digits10 is only 1 below max
    constexpr std::size_t buffsize = 32;
    static_assert(2 + std::numeric_limits<double>::digits10 < buffsize, "");

    char buff[buffsize];

    auto res = std::to_chars(buff, buff + buffsize, integer);

    // Can only fail becuase of buffer being too small
    BOOST_ASSERT(res.ec == std::errc());

    // Copy
    output.append(string_view(buff, res.ptr - buff));
}

inline error_code append_double(output_string_ref output, double number)
{
    // Make sure our buffer is big enough. 4: sign, radix point, e+
    // 3: max exponent digits
    constexpr std::size_t buffsize = 32;
    static_assert(4 + std::numeric_limits<double>::max_digits10 + 3 < buffsize, "");

    // inf and nan are not supported by MySQL
    if (std::isinf(number) || std::isnan(number))
        return client_errc::floating_point_nan_inf;

    char buff[buffsize];

    // We format as scientific to make MySQL understand the number as a double.
    // Otherwise, it takes it as a DECIMAL.
    auto res = std::to_chars(buff, buff + buffsize, number, std::chars_format::scientific);

    // Can only fail becuase of buffer being too small
    BOOST_ASSERT(res.ec == std::errc());

    // Copy
    output.append(string_view(buff, res.ptr - buff));

    return error_code();
}

inline error_code append_quoted_string(output_string_ref output, string_view str, const format_options& opts)
{
    output.append("'");
    auto ec = detail::escape_string(str, opts.charset, opts.backslash_escapes, '\'', output);
    if (ec)
        return ec;
    output.append("'");
    return error_code();
}

inline error_code append_quoted_blob(output_string_ref output, blob_view b, const format_options& opts)
{
    // Blobs have the same rules as strings
    return append_quoted_string(output, string_view(reinterpret_cast<const char*>(b.data()), b.size()), opts);
}

inline void append_quoted_date(output_string_ref output, date d)
{
    char buffer[34];
    buffer[0] = '\'';
    std::size_t sz = detail::date_to_string(d.year(), d.month(), d.day(), span<char, 32>(buffer + 1, 32));
    buffer[sz + 1] = '\'';
    output.append(string_view(buffer, sz + 2));
}

inline void append_quoted_datetime(output_string_ref output, datetime d)
{
    char buffer[66];
    buffer[0] = '\'';
    std::size_t sz = detail::datetime_to_string(
        d.year(),
        d.month(),
        d.day(),
        d.hour(),
        d.minute(),
        d.second(),
        d.microsecond(),
        span<char, 64>(buffer + 1, 64)
    );
    buffer[sz + 1] = '\'';
    output.append(string_view(buffer, sz + 2));
}

inline void append_quoted_time(output_string_ref output, time t)
{
    char buffer[66];
    buffer[0] = '\'';
    std::size_t sz = time_to_string(t, span<char, 64>(buffer + 1, 64));
    buffer[sz + 1] = '\'';
    output.append(string_view(buffer, sz + 2));
}

// Helpers for parsing format strings
inline bool is_number(char c) noexcept { return c >= '0' && c <= '9'; }

inline bool is_name_start(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

[[noreturn]] inline void throw_format_error(error_code ec, std::string diag)
{
    BOOST_THROW_EXCEPTION(error_with_diagnostics(ec, access::construct<diagnostics>(false, std::move(diag))));
}

[[noreturn]] inline void throw_format_error_invalid_arg(error_code ec)
{
    throw_format_error(ec, "Formatting SQL: an argument contains an invalid value");
}

class format_state
{
    format_context ctx_;
    span<const format_arg_descriptor> args_;

    // Borrowed from fmt
    // 0: we haven't used any args yet
    // -1: we're doing explicit indexing
    // >0: we're doing auto indexing
    int next_arg_id_{0};

    const char* advance(const char* begin, const char* end)
    {
        std::size_t size = ctx_.impl_.opts.charset.next_char({begin, end});
        if (size == 0)
        {
            throw_format_error(
                client_errc::invalid_encoding,
                "Formatting SQL: the format string contains characters that are invalid in the given "
                "character set"
            );
        }
        return begin + size;
    }

    bool uses_auto_ids() const noexcept { return next_arg_id_ > 0; }
    bool uses_explicit_ids() const noexcept { return next_arg_id_ == -1; }

    void do_field(format_arg_descriptor arg)
    {
        auto ec = ctx_.format_arg(arg.value);
        if (ec)
            throw_format_error_invalid_arg(ec);
    }

    void do_indexed_field(int arg_id)
    {
        BOOST_ASSERT(arg_id >= 0);
        if (static_cast<std::size_t>(arg_id) >= args_.size())
            throw_format_error(client_errc::invalid_format_string, "Formatting SQL: argument not found");
        do_field(args_[arg_id]);
    }

    const char* parse_field(const char* format_begin, const char* format_end)
    {
        // {{ : escape for brace
        // {}:  auto field
        // {integer}: index
        // {identifier}: named field
        // All characters until the closing } must be ASCII, otherwise the format string is not valid

        auto it = format_begin;
        if (it == format_end)
        {
            throw_format_error(
                client_errc::invalid_format_string,
                "Formatting SQL: unmatched '{' in format string"
            );
        }
        else if (*it == '{')
        {
            ctx_.append_raw("{");
            return ++it;
        }
        else if (*it == '}')
        {
            append_auto_field();
            return ++it;
        }
        else if (is_number(*it))
        {
            unsigned short field_index = 0;
            auto res = std::from_chars(it, format_end, field_index);
            if (res.ec != std::errc{})
            {
                throw_format_error(
                    client_errc::invalid_format_string,
                    "Formatting SQL: invalid argument index"
                );
            }
            it = res.ptr;
            append_indexed_field(static_cast<int>(field_index));
        }
        else
        {
            const char* name_begin = it;
            if (!is_name_start(*it++))
            {
                throw_format_error(
                    client_errc::invalid_format_string,
                    "Formatting SQL: invalid argument name"
                );
            }
            while (it != format_end && (is_name_start(*it) || is_number(*it)))
                ++it;
            append_named_field(string_view{name_begin, it});
        }

        if (it == format_end || *it++ != '}')
        {
            throw_format_error(client_errc::invalid_format_string, "Formatting SQL: invalid format string");
        }
        return it;
    }

    void append_named_field(string_view field_name)
    {
        // Find the argument
        for (const auto& arg : args_)
        {
            if (arg.name == field_name)
            {
                do_field(arg);
                return;
            }
        }

        // Not found
        throw_format_error(client_errc::invalid_format_string, "Formatting SQL: named argument not found");
    }

    void append_indexed_field(int index)
    {
        if (uses_auto_ids())
        {
            throw_format_error(
                client_errc::invalid_format_string,
                "Cannot switch from automatic to explicit indexing"
            );
        }
        next_arg_id_ = -1;
        do_indexed_field(index);
    }

    void append_auto_field()
    {
        if (uses_explicit_ids())
        {
            throw_format_error(
                client_errc::invalid_format_string,
                "Cannot switch from explicit to automatic indexing"
            );
        }
        do_indexed_field(next_arg_id_++);
    }

public:
    format_state(const format_context& ctx, span<const format_arg_descriptor> args) noexcept
        : ctx_(ctx), args_(args)
    {
    }

    void format(string_view format_str)
    {
        // We can use operator++ when we know a character is ASCII. Some charsets
        // allow ASCII continuation bytes, so we need to skip the entire character othwerwise
        auto cur_begin = format_str.data();
        auto it = format_str.data();
        auto end = format_str.data() + format_str.size();
        while (it != end)
        {
            if (*it == '{')
            {
                // Found a replacement field. Dump the SQL we've been skipping and parse it
                ctx_.append_raw({cur_begin, it});
                ++it;
                it = parse_field(it, end);
                cur_begin = it;
            }
            else if (*it == '}')
            {
                // A lonely } is only legal as a escape curly brace (i.e. }})
                ctx_.append_raw({cur_begin, it});
                ++it;
                if (it == end || *it != '}')
                    BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: unmatched '}'"));
                ctx_.append_raw("}");
                ++it;
                cur_begin = it;
            }
            else
            {
                it = advance(it, end);
            }
        }

        // Dump any remaining SQL
        ctx_.append_raw({cur_begin, end});
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

boost::mysql::error_code boost::mysql::formatter<boost::mysql::identifier>::format(
    const identifier& value,
    format_context& ctx
)
{
    ctx.append_raw("`");
    auto ec = detail::append_identifier(value.first(), ctx);
    if (ec)
        return ec;
    if (!value.second().empty())
    {
        ctx.append_raw("`.`");
        ec = detail::append_identifier(value.second(), ctx);
        if (ec)
            return ec;
        if (!value.third().empty())
        {
            ctx.append_raw("`.`");
            ec = detail::append_identifier(value.third(), ctx);
            if (ec)
                return ec;
        }
    }
    ctx.append_raw("`");
    return error_code();
}

boost::mysql::error_code boost::mysql::format_context::format_arg(detail::format_arg_value arg)
{
    if (arg.is_custom)
    {
        return arg.data.custom.format_fn(arg.data.custom.obj, *this);
    }
    else
    {
        field_view fv = arg.data.fv;
        switch (fv.kind())
        {
        case field_kind::null: append_raw("NULL"); return error_code();
        case field_kind::int64: detail::append_int(impl_.output, fv.get_int64()); return error_code();
        case field_kind::uint64: detail::append_int(impl_.output, fv.get_uint64()); return error_code();
        case field_kind::float_:
            // float is formatted as double because it's parsed as such
            return detail::append_double(impl_.output, fv.get_float());
        case field_kind::double_: return detail::append_double(impl_.output, fv.get_double());
        case field_kind::string:
            return detail::append_quoted_string(impl_.output, fv.get_string(), impl_.opts);
        case field_kind::blob: return detail::append_quoted_blob(impl_.output, fv.get_blob(), impl_.opts);
        case field_kind::date: detail::append_quoted_date(impl_.output, fv.get_date()); return error_code();
        case field_kind::datetime:
            detail::append_quoted_datetime(impl_.output, fv.get_datetime());
            return error_code();
        case field_kind::time: detail::append_quoted_time(impl_.output, fv.get_time()); return error_code();
        default: BOOST_ASSERT(false); return error_code();
        }
    }
}

void boost::mysql::format_context::on_format_arg_error(error_code ec)
{
    detail::throw_format_error_invalid_arg(ec);
}

void boost::mysql::detail::vformat_sql_to(
    string_view format_str,
    const format_context& ctx,
    span<const detail::format_arg_descriptor> args
)
{
    detail::format_state(ctx, args).format(format_str);
}

#endif
