//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DECIMAL_HPP
#define BOOST_MYSQL_DECIMAL_HPP

#include <boost/mysql/detail/config.hpp>

#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata.hpp>

#include <boost/mysql/detail/typing/readable_field_traits.hpp>

#include <boost/decimal.hpp>

#include <system_error>

namespace boost {
namespace mysql {
namespace detail {

// Get the number of decimal digits required to represent the given column.
// The server gives this information in required displayed characters,
// but there's a one-to-one mapping to precision.
// This same algorithm is employed by the server.
// Returns -1 in case of error
inline int get_decimal_precision(const metadata& meta)
{
    constexpr unsigned max_precision = 65u;                 // Max value allowed by the server
    unsigned radix_chars = meta.decimals() > 0u ? 1u : 0u;  // Number of characters used for the decimal point
    unsigned sign_chars = meta.is_unsigned() ? 0u : 1u;     // Number of characters used for the sign
    unsigned res = meta.column_length() - radix_chars - sign_chars;
    return res > max_precision ? -1 : static_cast<int>(res);
}

// meta_check implementation for decimal types
inline bool meta_check_decimal_impl(meta_check_context& ctx, int cpp_precision, const char* cpp_type_name)
{
    // Check the number of decimals
    int required_precision = get_decimal_precision(ctx.current_meta());
    if (required_precision == -1)
    {
        ctx.add_error() << "Invalid precision received from the server for decimal column: '"
                        << column_type_to_str(ctx.current_meta()) << "'";  // TODO: better msg
    }
    if (required_precision > cpp_precision)
    {
        auto& stream = ctx.add_error();
        stream << "Incompatible types for field ";
        ctx.insert_field_name(stream);
        stream << ": C++ type '" << cpp_type_name << "' has a precision of " << cpp_precision
               << " decimals, while the DB type requires a precision of " << required_precision
               << " decimals";
    }

    // Check type (encoded as this function's return value)
    return ctx.current_meta().type() == column_type::decimal;
}

// parse implementation for decimal types
template <class Decimal>
error_code parse_decimal_impl(field_view input, Decimal& output)
{
    // Check type
    if (!input.is_string())
        return client_errc::static_row_parsing_error;
    auto str = input.get_string();

    // Invoke decimal's charconv. MySQL always uses the fixed format.
    auto res = decimal::from_chars(str.begin(), str.end(), output, decimal::chars_format::fixed);
    if (res.ec != std::errc{} || res.ptr != str.end())
        return client_errc::static_row_parsing_error;

    // Done
    return error_code();
}

template <>
struct readable_field_traits<decimal::decimal32, void>
{
    static constexpr bool is_supported = true;
    static BOOST_INLINE_CONSTEXPR const char* type_name = "decimal32";
    static bool meta_check(meta_check_context& ctx) { return meta_check_decimal_impl(ctx, 7, type_name); }
    static error_code parse(field_view input, decimal::decimal32& output)
    {
        return parse_decimal_impl(input, output);
    }
};

template <>
struct readable_field_traits<decimal::decimal64, void>
{
    static constexpr bool is_supported = true;
    static BOOST_INLINE_CONSTEXPR const char* type_name = "decimal64";
    static bool meta_check(meta_check_context& ctx) { return meta_check_decimal_impl(ctx, 16, type_name); }
    static error_code parse(field_view input, decimal::decimal64& output)
    {
        return parse_decimal_impl(input, output);
    }
};

template <>
struct readable_field_traits<decimal::decimal128, void>
{
    static constexpr bool is_supported = true;
    static BOOST_INLINE_CONSTEXPR const char* type_name = "decimal128";
    static bool meta_check(meta_check_context& ctx) { return meta_check_decimal_impl(ctx, 34, type_name); }
    static error_code parse(field_view input, decimal::decimal128& output)
    {
        return parse_decimal_impl(input, output);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

#endif
