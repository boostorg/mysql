//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_FORMAT_IPP
#define BOOST_MYSQL_IMPL_FORMAT_IPP

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/escape_string.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/format.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/format.hpp>
#include <boost/mysql/detail/output_string_ref.hpp>

#include <boost/mysql/impl/internal/time_to_string.hpp>

#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace boost {
namespace mysql {
namespace detail {

// Helpers to format fundamental types
inline void append_identifier(string_view name, format_context& ctx)
{
    auto& impl = access::get_impl(ctx);
    auto ec = detail::escape_string(
        name,
        impl.opts.charset,
        impl.opts.backslash_escapes,
        quoting_context::backtick,
        impl.output
    );
    if (ec)
    {
        BOOST_THROW_EXCEPTION(boost::system::system_error(ec));
    }
}

template <class T>
void append_number(output_string_ref output, T number)
{
    // TODO: we can probably do this better
    output.append(std::to_string(number));
}

inline void append_quoted_string(output_string_ref output, string_view str, const format_options& opts)
{
    output.append("'");
    auto ec = detail::escape_string(
        str,
        opts.charset,
        opts.backslash_escapes,
        quoting_context::single_quote,
        output
    );
    if (ec)
        BOOST_THROW_EXCEPTION(boost::system::system_error(ec));
    output.append("'");
}

inline void append_quoted_blob(output_string_ref output, blob_view b, const format_options& opts)
{
    // Blobs have the same rules as strings
    append_quoted_string(output, string_view(reinterpret_cast<const char*>(b.data()), b.size()), opts);
}

inline void append_quoted_date(output_string_ref output, date d)
{
    char buffer[34];
    buffer[0] = '\'';
    std::size_t sz = access::get_impl(d).to_string(span<char, 32>(buffer + 1, 32));
    buffer[sz + 1] = '\'';
    output.append(string_view(buffer, sz + 2));
}

inline void append_quoted_datetime(output_string_ref output, datetime d)
{
    char buffer[66];
    buffer[0] = '\'';
    std::size_t sz = access::get_impl(d).to_string(span<char, 64>(buffer + 1, 64));
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
inline const char* advance(const char* begin, const char* end, const character_set& charset)
{
    int size = charset.next_char({begin, end});
    if (size == 0)
        BOOST_THROW_EXCEPTION(boost::system::system_error(client_errc::invalid_encoding));
    return begin + size;
}

inline bool is_number(char c) noexcept { return c >= '0' && c <= '9'; }

inline bool is_name_start(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
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

    bool uses_auto_ids() const noexcept { return next_arg_id_ > 0; }
    bool uses_explicit_ids() const noexcept { return next_arg_id_ == -1; }

    void do_field(format_arg_descriptor arg) { ctx_.format_arg(arg.value); }

    void do_indexed_field(int arg_id)
    {
        BOOST_ASSERT(arg_id >= 0);
        if (static_cast<std::size_t>(arg_id) >= args_.size())
            BOOST_THROW_EXCEPTION(std::runtime_error("Format argument not found"));
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
            BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: unmatched '{'"));
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
                BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: bad index"));
            }
            it = res.ptr;
            append_indexed_field(static_cast<int>(field_index));
        }
        else
        {
            const char* name_begin = it;
            if (!is_name_start(*it++))
                BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: invalid argument name"));
            while (it != format_end && (is_name_start(*it) || is_number(*it)))
                ++it;
            append_named_field(string_view{name_begin, it});
        }

        if (it == format_end || *it++ != '}')
            BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: unmatched '}"));
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

        // Not found. TODO: better diagnostics
        BOOST_THROW_EXCEPTION(std::runtime_error("Format argument not found"));
    }

    void append_indexed_field(int index)
    {
        if (uses_auto_ids())
            BOOST_THROW_EXCEPTION(std::runtime_error("Cannot switch from automatic to explicit indexing"));
        next_arg_id_ = -1;
        do_indexed_field(index);
    }

    void append_auto_field()
    {
        if (uses_explicit_ids())
            BOOST_THROW_EXCEPTION(std::runtime_error("Cannot switch from explicit to automatic indexing"));
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
                it = advance(it, end, ctx_.impl_.opts.charset);
            }
        }

        // Dump any remaining SQL
        ctx_.append_raw({cur_begin, end});
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

void boost::mysql::formatter<boost::mysql::identifier>::format(const identifier& value, format_context& ctx)
{
    ctx.append_raw("`");
    detail::append_identifier(value.first(), ctx);
    if (!value.second().empty())
    {
        ctx.append_raw("`.`");
        detail::append_identifier(value.second(), ctx);
        if (!value.third().empty())
        {
            ctx.append_raw("`.`");
            detail::append_identifier(value.third(), ctx);
        }
    }
    ctx.append_raw("`");
}

void boost::mysql::format_context::format_arg(detail::format_arg_value arg)
{
    if (arg.is_custom)
    {
        arg.data.custom.format_fn(arg.data.custom.obj, *this);
    }
    else
    {
        field_view fv = arg.data.fv;
        switch (fv.kind())
        {
        case field_kind::null: return append_raw("NULL");
        case field_kind::int64: return detail::append_number(impl_.output, fv.get_int64());
        case field_kind::uint64: return detail::append_number(impl_.output, fv.get_uint64());
        case field_kind::float_: return detail::append_number(impl_.output, fv.get_float());
        case field_kind::double_: return detail::append_number(impl_.output, fv.get_double());
        case field_kind::string:
            return detail::append_quoted_string(impl_.output, fv.get_string(), impl_.opts);
        case field_kind::blob: return detail::append_quoted_blob(impl_.output, fv.get_blob(), impl_.opts);
        case field_kind::date: return detail::append_quoted_date(impl_.output, fv.get_date());
        case field_kind::datetime: return detail::append_quoted_datetime(impl_.output, fv.get_datetime());
        case field_kind::time: return detail::append_quoted_time(impl_.output, fv.get_time());
        default: BOOST_ASSERT(false);
        }
    }
}

void boost::mysql::detail::vformat_to(
    string_view format_str,
    const format_context& ctx,
    span<const detail::format_arg_descriptor> args
)
{
    detail::format_state(ctx, args).format(format_str);
}

#endif
