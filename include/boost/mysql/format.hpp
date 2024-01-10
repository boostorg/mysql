//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FORMAT_HPP
#define BOOST_MYSQL_FORMAT_HPP

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/escape_string.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/time.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/dt_to_string.hpp>
#include <boost/mysql/detail/output_string_ref.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/config/detail/suffix.hpp>
#include <boost/core/span.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>

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
struct formatter;

class format_context;

enum class arg_kind
{
    null,
    int64,
    uint64,
    string,
    float_,
    double_,
    date,
    datetime,
    time,
    raw,
    custom,
};

struct arg_value
{
    struct any_custom_arg
    {
        const void* obj;
        void (*format_fn)(const void*, format_context&);
    };

    arg_kind kind;
    union data_t
    {
        std::nullptr_t null;
        std::int64_t int64;
        std::uint64_t uint64;
        string_view char_range;
        float float_;
        double double_;
        date date_;
        datetime datetime_;
        time time_;
        any_custom_arg custom;

        data_t() noexcept : null(nullptr) {}
        data_t(std::int64_t v) noexcept : int64(v) {}
        data_t(std::uint64_t v) noexcept : uint64(v) {}
        data_t(string_view v) noexcept : char_range(v) {}
        data_t(float v) noexcept : float_(v) {}
        data_t(double v) noexcept : double_(v) {}
        data_t(date d) noexcept : date_(d) {}
        data_t(datetime d) noexcept : datetime_(d) {}
        data_t(time v) noexcept : time_(v) {}
        data_t(any_custom_arg v) noexcept : custom(v) {}
    } data;
};

template <class T>
void do_custom_format(const void* obj, format_context& ctx)
{
    formatter<T>::format(*static_cast<const T*>(obj), ctx);
}

inline string_view to_string_view(blob_view value) noexcept
{
    return string_view(reinterpret_cast<const char*>(value.data()), value.size());
}

template <class T>
arg_value create_arg_value_impl(const T& v, std::true_type) noexcept
{
    field_view fv = detail::writable_field_traits<T>::to_field(v);
    switch (fv.kind())
    {
    case field_kind::int64: return {arg_kind::int64, arg_value::data_t(fv.get_int64())};
    case field_kind::uint64: return {arg_kind::uint64, arg_value::data_t(fv.get_uint64())};
    case field_kind::float_: return {arg_kind::float_, arg_value::data_t(fv.get_float())};
    case field_kind::double_: return {arg_kind::double_, arg_value::data_t(fv.get_double())};
    case field_kind::string: return {arg_kind::string, arg_value::data_t(fv.get_string())};
    case field_kind::blob: return {arg_kind::string, arg_value::data_t(to_string_view(fv.get_blob()))};
    case field_kind::date: return {arg_kind::date, arg_value::data_t(fv.get_date())};
    case field_kind::datetime: return {arg_kind::datetime, fv.get_datetime()};
    case field_kind::time: return {arg_kind::time, fv.get_time()};
    case field_kind::null:
    default: return {arg_kind::null, arg_value::data_t()};
    }
}

template <class T>
arg_value create_arg_value_impl(const T& v, std::false_type) noexcept
{
    return {arg_kind::custom, arg_value::data_t({&v, &do_custom_format<T>})};
}

template <class T>
arg_value create_arg_value(const T& v) noexcept
{
    return create_arg_value_impl(v, detail::is_writable_field<T>{});
}

inline arg_value create_arg_value(raw_sql v) noexcept { return {arg_kind::raw, v.get()}; }

class format_context
{
    detail::output_string_ref output_;
    character_set charset_;
    bool backslash_escapes_;

    friend class query_builder;

    // Formatters for the different types. THis should be in TU
    void format_null() { append_raw("NULL"); }

    // TODO: can we make these better
    void format_int64(std::int64_t value) { append_raw(std::to_string(value)); }
    void format_uint64(std::uint64_t value) { append_raw(std::to_string(value)); }
    void format_float(float value) { append_raw(std::to_string(value)); }
    void format_double(double value) { append_raw(std::to_string(value)); }

    void format_string(string_view value)
    {
        append_raw("'");
        auto ec = detail::escape_string(
            value,
            charset_,
            backslash_escapes_,
            quoting_context::single_quote,
            output_
        );
        if (ec)
            BOOST_THROW_EXCEPTION(boost::system::system_error(ec));
        append_raw("'");
    }

    void format_date(date d)
    {
        char buffer[34];
        buffer[0] = '\'';
        std::size_t sz = detail::format_date(d.year(), d.month(), d.day(), span<char, 32>(buffer + 1, 32));
        buffer[sz + 1] = '\'';
        append_raw(string_view(buffer, sz + 2));
    }

    void format_datetime(const datetime& d)
    {
        char buffer[66];
        buffer[0] = '\'';
        std::size_t sz = detail::format_datetime(
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
        append_raw(string_view(buffer, sz + 2));
    }

    void format_time(time t)
    {
        char buffer[66];
        buffer[0] = '\'';
        std::size_t sz = detail::format_time(t, span<char, 64>(buffer + 1, 64));
        buffer[sz + 1] = '\'';
        append_raw(string_view(buffer, sz + 2));
    }

    void format_custom(arg_value::any_custom_arg value) { value.format_fn(value.obj, *this); }

    BOOST_MYSQL_DECL
    void format_arg(arg_value arg)
    {
        switch (arg.kind)
        {
        case arg_kind::null: return format_null();
        case arg_kind::int64: return format_int64(arg.data.int64);
        case arg_kind::uint64: return format_uint64(arg.data.uint64);
        case arg_kind::float_: return format_float(arg.data.float_);
        case arg_kind::double_: return format_double(arg.data.double_);
        case arg_kind::string: return format_string(arg.data.char_range);
        case arg_kind::date: return format_date(arg.data.date_);
        case arg_kind::datetime: return format_datetime(arg.data.datetime_);
        case arg_kind::time: return format_time(arg.data.time_);
        case arg_kind::raw: return append_raw(arg.data.char_range);
        case arg_kind::custom: return format_custom(arg.data.custom);
        default: return;
        }
    }

public:
    format_context(
        detail::output_string_ref out,
        const character_set& charset,
        bool backslash_escapes
    ) noexcept
        : output_(out), charset_(charset), backslash_escapes_(backslash_escapes)
    {
    }
    detail::output_string_ref output() noexcept { return output_; }

    const character_set& charset() const noexcept { return charset_; }
    bool backslash_slashes() const noexcept { return backslash_escapes_; }

    void append_raw(string_view raw_sql) { output_.append(raw_sql); }

    template <class T>
    void append_value(const T& v)
    {
        format_arg(create_arg_value(v));
    }
};

template <>
struct formatter<identifier>
{
    static void append_identifier(string_view name, format_context& ctx)
    {
        BOOST_ASSERT(!name.empty());
        auto ec = detail::escape_string(
            name,
            ctx.charset(),
            ctx.backslash_slashes(),
            quoting_context::backtick,
            ctx.output()
        );
        if (ec)
        {
            BOOST_THROW_EXCEPTION(boost::system::system_error(ec));
        }
    }

    static void format(const identifier& value, format_context& ctx)
    {
        ctx.append_raw("`");
        append_identifier(value.first(), ctx);
        if (!value.second().empty())
        {
            ctx.append_raw("`.`");
            append_identifier(value.second(), ctx);
            if (!value.third().empty())
            {
                ctx.append_raw("`.`");
                append_identifier(value.third(), ctx);
            }
        }
        ctx.append_raw("`");
    }
};

struct arg_descriptor
{
    arg_value value;
    string_view name;
};

inline arg_descriptor make_arg_descriptor(const arg_descriptor& v) noexcept { return v; }

template <class T>
arg_descriptor make_arg_descriptor(const T& val)
{
    return {create_arg_value(val), {}};
}

template <class T>
inline arg_descriptor arg(const T& value, string_view name) noexcept
{
    return {create_arg_value(value), name};
}

template <class... T>
std::array<arg_descriptor, sizeof...(T)> make_arg_descriptors(const T&... args)
{
    return {{make_arg_descriptor<T>(args)...}};
}

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

class query_builder
{
    format_context ctx_;
    span<const arg_descriptor> args_;

    // Borrowed from fmt
    // 0: we haven't used any args yet
    // -1: we're doing explicit indexing
    // >0: we're doing auto indexing
    int next_arg_id_{0};

    bool uses_auto_ids() const noexcept { return next_arg_id_ > 0; }
    bool uses_explicit_ids() const noexcept { return next_arg_id_ == -1; }

    void do_field(arg_descriptor arg) { ctx_.format_arg(arg.value); }

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
    query_builder(
        detail::output_string_ref output,
        const character_set& charset,
        bool backslash_escapes,
        span<const arg_descriptor> args
    ) noexcept
        : ctx_(output, charset, backslash_escapes), args_(args)
    {
    }

    void format(const char* format_begin, const char* format_end)
    {
        // We can use operator++ when we know a character is ASCII. Some charsets
        // allow ASCII continuation bytes, so we need to skip the entire character othwerwise
        auto cur_begin = format_begin;
        auto it = format_begin;
        while (it != format_end)
        {
            if (*it == '{')
            {
                // Found a replacement field. Dump the SQL we've been skipping and parse it
                ctx_.append_raw({cur_begin, it});
                ++it;
                it = parse_field(it, format_end);
                cur_begin = it;
            }
            else if (*it == '}')
            {
                // A lonely } is only legal as a escape curly brace (i.e. }})
                ctx_.append_raw({cur_begin, it});
                ++it;
                if (it == format_end || *it != '}')
                    BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: unmatched '}'"));
                ctx_.append_raw("}");
                ++it;
                cur_begin = it;
            }
            else
            {
                it = advance(it, format_end, ctx_.charset_);
            }
        }

        // Dump any remaining SQL
        ctx_.append_raw({cur_begin, format_end});
    }
};

BOOST_MYSQL_DECL
void vformat_to(
    string_view format_str,
    detail::output_string_ref output,
    const character_set& charset,
    bool backslash_escapes,
    span<const arg_descriptor> args
)
{
    query_builder builder(output, charset, backslash_escapes, args);
    const char* begin = format_str.data();
    const char* end = begin + format_str.size();
    builder.format(begin, end);
}

/**
 * PUBLIC
 */

struct format_options
{
    character_set charset;
    bool backslash_escapes;
};

template <BOOST_MYSQL_OUTPUT_STRING Output, class... Args>
inline void format_to(string_view format_str, Output& output, const format_options& opts, const Args&... args)
{
    vformat_to(
        format_str,
        detail::output_string_ref::create(output),
        opts.charset,
        opts.backslash_escapes,
        make_arg_descriptors(args...)
    );
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

#endif
