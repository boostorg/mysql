//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_FORMAT_SQL_IPP
#define BOOST_MYSQL_IMPL_FORMAT_SQL_IPP

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/character_set.hpp>
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
#include <boost/mysql/detail/output_string.hpp>

#include <boost/mysql/impl/internal/dt_to_string.hpp>

#include <boost/charconv/from_chars.hpp>
#include <boost/charconv/to_chars.hpp>
#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

// Helpers to format fundamental types
inline void append_identifier(string_view name, format_context_base& ctx)
{
    auto& impl = access::get_impl(ctx);
    auto ec = detail::escape_string(name, impl.opts, '`', impl.output);
    if (ec)
        ctx.add_error(ec);
}

template <class T>
void append_int(output_string_ref output, T integer)
{
    // Make sure our buffer is big enough. 2: sign + digits10 is only 1 below max
    constexpr std::size_t buffsize = 32;
    static_assert(2 + std::numeric_limits<double>::digits10 < buffsize, "");

    char buff[buffsize];

    auto res = charconv::to_chars(buff, buff + buffsize, integer);

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
    auto res = charconv::to_chars(buff, buff + buffsize, number, charconv::chars_format::scientific);

    // Can only fail because of buffer being too small
    BOOST_ASSERT(res.ec == std::errc());

    // Copy
    output.append(string_view(buff, res.ptr - buff));

    return error_code();
}

inline error_code append_quoted_string(output_string_ref output, string_view str, const format_options& opts)
{
    output.append("'");
    auto ec = detail::escape_string(str, opts, '\'', output);
    output.append("'");
    return ec;
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

inline error_code append_field_view(output_string_ref output, field_view fv, const format_options& opts)
{
    switch (fv.kind())
    {
    case field_kind::null: output.append("NULL"); return error_code();
    case field_kind::int64: append_int(output, fv.get_int64()); return error_code();
    case field_kind::uint64: append_int(output, fv.get_uint64()); return error_code();
    case field_kind::float_:
        // float is formatted as double because it's parsed as such
        return append_double(output, fv.get_float());
    case field_kind::double_: return append_double(output, fv.get_double());
    case field_kind::string: return append_quoted_string(output, fv.get_string(), opts);
    case field_kind::blob: return append_quoted_blob(output, fv.get_blob(), opts);
    case field_kind::date: append_quoted_date(output, fv.get_date()); return error_code();
    case field_kind::datetime: append_quoted_datetime(output, fv.get_datetime()); return error_code();
    case field_kind::time: append_quoted_time(output, fv.get_time()); return error_code();
    default: BOOST_ASSERT(false); return error_code();
    }
}

// Helpers for parsing format strings
inline bool is_number(char c) noexcept { return c >= '0' && c <= '9'; }

inline bool is_name_start(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

class format_state
{
    format_context_base& ctx_;
    span<const format_arg> args_;

    // Borrowed from fmt
    // 0: we haven't used any args yet
    // -1: we're doing explicit indexing
    // >0: we're doing auto indexing
    int next_arg_id_{0};

    [[noreturn]] static void throw_format_error(std::string diag)
    {
        BOOST_THROW_EXCEPTION(error_with_diagnostics(
            client_errc::invalid_format_string,
            access::construct<diagnostics>(false, std::move(diag))
        ));
    }

    [[noreturn]] static void throw_invalid_format_string()
    {
        throw_format_error("Formatting SQL: invalid format string");
    }

    const char* advance(const char* begin, const char* end)
    {
        std::size_t size = ctx_.impl_.opts.charset.next_char({begin, end});
        if (size == 0)
        {
            throw_format_error(
                "Formatting SQL: the format string contains characters that are invalid in the given "
                "character set"
            );
        }
        return begin + size;
    }

    bool uses_auto_ids() const noexcept { return next_arg_id_ > 0; }
    bool uses_explicit_ids() const noexcept { return next_arg_id_ == -1; }

    void do_field(format_arg arg) { ctx_.format_arg(arg.value); }

    void do_indexed_field(int arg_id)
    {
        BOOST_ASSERT(arg_id >= 0);
        if (static_cast<std::size_t>(arg_id) >= args_.size())
            throw_format_error("Formatting SQL: argument index out of range");
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
            throw_invalid_format_string();
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
            auto res = charconv::from_chars(it, format_end, field_index);
            if (res.ec != std::errc{})
            {
                throw_invalid_format_string();
            }
            it = res.ptr;
            if (it == format_end || *it++ != '}')
            {
                throw_invalid_format_string();
            }
            append_indexed_field(static_cast<int>(field_index));
        }
        else
        {
            const char* name_begin = it;
            if (!is_name_start(*it++))
            {
                throw_invalid_format_string();
            }
            while (it != format_end && (is_name_start(*it) || is_number(*it)))
                ++it;
            string_view field_name(name_begin, it);
            if (it == format_end || *it++ != '}')
            {
                throw_invalid_format_string();
            }
            append_named_field(field_name);
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
        throw_format_error("Formatting SQL: named argument not found");
    }

    void append_indexed_field(int index)
    {
        if (uses_auto_ids())
        {
            throw_format_error("Formatting SQL: cannot switch from automatic to explicit indexing");
        }
        next_arg_id_ = -1;
        do_indexed_field(index);
    }

    void append_auto_field()
    {
        if (uses_explicit_ids())
        {
            throw_format_error("Formatting SQL: cannot switch from explicit to automatic indexing");
        }
        do_indexed_field(next_arg_id_++);
    }

public:
    format_state(format_context_base& ctx, span<const format_arg> args) noexcept : ctx_(ctx), args_(args) {}

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
                ctx_.impl_.output.append({cur_begin, it});
                ++it;
                it = parse_field(it, end);
                cur_begin = it;
            }
            else if (*it == '}')
            {
                // A lonely } is only legal as a escape curly brace (i.e. }})
                ctx_.impl_.output.append({cur_begin, it});
                ++it;
                if (it == end || *it != '}')
                    throw_format_error("Formatting SQL: unbalanced '}' in format string");
                ctx_.impl_.output.append("}");
                ++it;
                cur_begin = it;
            }
            else
            {
                it = advance(it, end);
            }
        }

        // Dump any remaining SQL
        ctx_.impl_.output.append({cur_begin, end});
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

void boost::mysql::formatter<boost::mysql::identifier>::format(
    const identifier& value,
    format_context_base& ctx
)
{
    const auto& impl = detail::access::get_impl(value);

    ctx.append_raw("`");
    detail::append_identifier(impl.ids[0], ctx);
    if (impl.qual_level >= 2u)
    {
        ctx.append_raw("`.`");
        detail::append_identifier(impl.ids[1], ctx);
        if (impl.qual_level == 3u)
        {
            ctx.append_raw("`.`");
            detail::append_identifier(impl.ids[2], ctx);
        }
    }
    ctx.append_raw("`");
}

void boost::mysql::format_context_base::format_arg(detail::format_arg_value arg)
{
    if (arg.is_custom)
    {
        arg.data.custom.format_fn(arg.data.custom.obj, *this);
    }
    else
    {
        error_code ec = detail::append_field_view(impl_.output, arg.data.fv, impl_.opts);
        if (ec)
            add_error(ec);
    }
}

void boost::mysql::detail::vformat_sql_to(
    string_view format_str,
    format_context_base& ctx,
    span<const detail::format_arg> args
)
{
    detail::format_state(ctx, args).format(format_str);
}

std::string boost::mysql::detail::check_format_result(system::result<std::string>&& res)
{
    if (res.has_error())
    {
        BOOST_THROW_EXCEPTION(system::system_error(res.error(), "Formatting SQL"));
    }
    return std::move(res).value();
}

#endif
