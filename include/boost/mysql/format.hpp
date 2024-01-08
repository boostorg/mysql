//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FORMAT_HPP
#define BOOST_MYSQL_FORMAT_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/escape_string.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/config/detail/suffix.hpp>
#include <boost/core/span.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>

namespace boost {
namespace mysql {

// TODO: this is detail
struct format_options
{
    const character_set& charset;
    bool backslash_escapes;
};

struct arg_descriptor
{
    const void* arg;
    void (*format_fn)(const void* value, std::string&, format_options);
    string_view name;
};

template <class T>
struct named_arg
{
    string_view name;
    const T& value;
};

template <class T>
struct formatter;

template <class T>
void do_format(const void* value, std::string& to, format_options opts)
{
    formatter<T>::format(*static_cast<const T*>(value), to, opts);
}

template <class T>
arg_descriptor make_arg_descriptor(const T& val, string_view name = {})
{
    return {&val, formatter<T>::format, name};
}

template <class T>
arg_descriptor make_arg_descriptor(const named_arg<T>& val)
{
    return make_arg_descriptor(val.value, val.name);
}

template <class... T>
std::array<arg_descriptor, sizeof...(T)> make_arg_descriptors(const T&... args)
{
    return {{make_arg_descriptor<T>(args)...}};
}

template <>
struct formatter<std::int64_t>
{
    inline static void format(std::int64_t value, std::string& to, format_options)
    {
        // TODO: can we make this better
        auto str = std::to_string(value);
        to.append(str);
    }
};

template <>
struct formatter<float>
{
    inline static void format(float value, std::string& to, format_options)
    {
        auto str = std::to_string(value);
        to.append(str);
    }
};

template <>
struct formatter<string_view>
{
    inline static void format(string_view value, std::string& to, format_options opts)
    {
        // TODO: we need to make escape_string not clear the output
        to.push_back('\'');
        auto ec = escape_string(
            value,
            opts.charset,
            opts.backslash_escapes,
            quoting_context::single_quote,
            to
        );
        if (ec)
            BOOST_THROW_EXCEPTION(boost::system::system_error(ec));
        to.push_back('\'');
    }
};

// inline string_view get_next_char(string_view input, const character_set& charset)
// {
//     int size = charset.next_char(input);
//     if (size == 0)
//         BOOST_THROW_EXCEPTION(boost::system::system_error(client_errc::invalid_encoding));
//     return input.substr(static_cast<std::size_t>(size));
// }

class query_builder
{
    const format_options opts_;
    std::string& output_;
    span<const arg_descriptor> args_;

    // Borrowed from fmt
    // 0: we haven't used any args yet
    // -1: we're doing explicit indexing
    // >0: we're doing auto indexing
    int next_arg_id_{0};

    bool uses_auto_ids() const noexcept { return next_arg_id_ > 0; }

    void do_field(arg_descriptor arg) { arg.format_fn(arg.arg, output_, opts_); }

    void do_indexed_field(int arg_id)
    {
        BOOST_ASSERT(arg_id >= 0);
        if (static_cast<std::size_t>(arg_id) >= args_.size())
            BOOST_THROW_EXCEPTION(std::runtime_error("Format argument not found"));
        do_field(args_[arg_id]);
    }

public:
    query_builder(format_options opts, std::string& output, span<const arg_descriptor> args) noexcept
        : opts_(opts), output_(output), args_(args)
    {
    }

    void sql(string_view raw_sql) { output_.append(raw_sql); }

    void field(string_view field_name)
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

    void field(int index)
    {
        if (uses_auto_ids())
            BOOST_THROW_EXCEPTION(std::runtime_error("Cannot switch from automatic to explicit indexing"));
        next_arg_id_ = -1;
        do_indexed_field(index);
    }

    void field()
    {
        if (!uses_auto_ids())
            BOOST_THROW_EXCEPTION(std::runtime_error("Cannot switch from explicit to automatic indexing"));
        do_indexed_field(next_arg_id_++);
    }
};

inline bool is_number(char c) noexcept { return c >= '0' && c <= '9'; }

inline bool is_name_start(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

inline const char* parse_field(const char* format_begin, const char* format_end, query_builder& builder)
{
    // {{ : escape for brace
    // {}:  auto field
    // {integer}: index
    // {identifier}: named field

    auto it = format_begin;
    if (it == format_end)
        BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: unmatched '{'"));
    else if (*it == '{')
    {
        builder.sql("{");
        return ++it;
    }
    else if (*it == '}')
    {
        builder.field();
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
        builder.field(static_cast<int>(field_index));
    }
    else
    {
        const char* name_begin = it;
        if (!is_name_start(*it++))
            BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: invalid argument name"));
        while (it != format_end && (is_name_start(*it) || is_number(*it)))
            ++it;
        builder.field(string_view{name_begin, it});
    }

    if (it == format_end || *it++ != '}')
        BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: unmatched '}"));
    return it;
}

inline void format_impl(const char* format_begin, const char* format_end, query_builder& builder)
{
    auto cur_begin = format_begin;
    auto it = format_begin;
    while (it != format_end)
    {
        if (*it == '{')
        {
            // Found a replacement field. Dump the SQL we've been skipping and parse it
            builder.sql({cur_begin, it});
            it = parse_field(++it, format_end, builder);
            cur_begin = it;
        }
        else if (*it++ == '}')
        {
            // A lonely } is only legal as a escape curly brace (i.e. }})
            builder.sql({cur_begin, it});
            if (it == format_end || *it++ != '}')
                BOOST_THROW_EXCEPTION(std::runtime_error("Bad format string: unmatched '}"));
            builder.sql("}");
            cur_begin = it;
        }
    }

    // Dump any remaining SQL
    builder.sql({cur_begin, format_end});
}

inline void vformat_to(
    string_view format_str,
    std::string& output,
    format_options opts,
    span<const arg_descriptor> args
)
{
    query_builder builder(opts, output, args);
    const char* begin = format_str.data();
    const char* end = begin + format_str.size();
    format_impl(begin, end, builder);
}

/**
 * PUBLIC
 */

template <class T>
BOOST_CXX14_CONSTEXPR named_arg<T> arg(string_view name, const T& value) noexcept
{
    return named_arg<T>(name, value);
}

// TODO: allow types that are not std::string
// TODO: compile-time safety
template <class... Args>
inline void format_to(string_view format_str, std::string& output, format_options opts, const Args&... args)
{
    auto desc = make_arg_descriptors(args...);
    vformat_to(format_str, output, opts, desc);
}

}  // namespace mysql
}  // namespace boost

#endif
