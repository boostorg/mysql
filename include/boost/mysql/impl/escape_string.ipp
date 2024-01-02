//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ESCAPE_STRING_IPP
#define BOOST_MYSQL_IMPL_ESCAPE_STRING_IPP

#pragma once

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/escape_string.hpp>

namespace boost {
namespace mysql {
namespace detail {

BOOST_ATTRIBUTE_NODISCARD
inline error_code duplicate_quotes(
    string_view input,
    const character_set& charset,
    quoting_context quot_ctx,
    std::string& output
)
{
    output.clear();
    output.reserve(input.size());
    char quote_char = static_cast<char>(quot_ctx);
    while (!input.empty())
    {
        std::size_t char_size = charset.next_char(input);
        BOOST_ASSERT(char_size <= 4u);
        if (char_size == 0u)
            return client_errc::invalid_encoding;
        auto cur_char = input.substr(0, char_size);
        if (cur_char.size() == 1u && cur_char[0] == quote_char)
            output.push_back(quote_char);
        output.append(cur_char.data(), cur_char.size());
        input = input.substr(char_size);
    }
    return error_code();
}

inline char get_escape(char input)
{
    switch (input)
    {
    case '\0': return '0';
    case '\n': return 'n';
    case '\r': return 'r';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    case '\x1a': return 'Z';  // Ctrl+Z
    default: return '\0';     // No escape
    }
}

BOOST_ATTRIBUTE_NODISCARD
inline error_code add_backslashes(string_view input, const character_set& charset, std::string& output)
{
    output.clear();
    output.reserve(input.size());
    while (!input.empty())
    {
        std::size_t char_size = charset.next_char(input);
        BOOST_ASSERT(char_size <= 4u);
        if (char_size == 0u)
            return client_errc::invalid_encoding;
        auto cur_char = input.substr(0, char_size);
        if (cur_char.size() == 1u)
        {
            char unescaped_char = cur_char[0];
            char escape = get_escape(unescaped_char);
            if (escape == '\0')
            {
                // No escape
                output.push_back(unescaped_char);
            }
            else
            {
                output.push_back('\\');
                output.push_back(escape);
            }
        }
        else
        {
            output.append(cur_char.data(), cur_char.size());
        }
        input = input.substr(char_size);
    }
    return error_code();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

boost::mysql::error_code boost::mysql::escape_string(
    string_view input,
    const character_set& charset,
    bool backslash_escapes,
    quoting_context quot_ctx,
    std::string& output
)
{
    return (quot_ctx == quoting_context::backtick || !backslash_escapes)
               ? detail::duplicate_quotes(input, charset, quot_ctx, output)
               : detail::add_backslashes(input, charset, output);
}

#endif
