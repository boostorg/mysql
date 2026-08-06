//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_DECIMAL_HPP
#define BOOST_MYSQL_DETAIL_TYPING_DECIMAL_HPP

#include <boost/mysql/detail/config.hpp>

#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/readable_field_traits.hpp>

#include <boost/decimal/charconv.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>

#include <system_error>

namespace boost {
namespace mysql {
namespace detail {

// Get the number of decimal digits required to represent the given column.
// The server gives this information in required displayed characters,
// but there's a one-to-one mapping to precision.
// This same algorithm is employed by the server.
// Returns -1 in case of error
inline int decimal_required_precision(const metadata& meta)
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
    int required_precision = decimal_required_precision(ctx.current_meta());
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

// type names for decimals
constexpr const char* decimal_type_name(decimal::decimal32_t) { return "decimal32_t"; }
constexpr const char* decimal_type_name(decimal::decimal64_t) { return "decimal64_t"; }
constexpr const char* decimal_type_name(decimal::decimal128_t) { return "decimal128_t"; }
constexpr const char* decimal_type_name(decimal::decimal_fast32_t) { return "decimal_fast32_t"; }
constexpr const char* decimal_type_name(decimal::decimal_fast64_t) { return "decimal_fast64_t"; }
constexpr const char* decimal_type_name(decimal::decimal_fast128_t) { return "decimal_fast128_t"; }

// precisions for decimals
constexpr int decimal_precision(decimal::decimal32_t) { return 7; }
constexpr int decimal_precision(decimal::decimal64_t) { return 16; }
constexpr int decimal_precision(decimal::decimal128_t) { return 34; }
constexpr int decimal_precision(decimal::decimal_fast32_t) { return 7; }
constexpr int decimal_precision(decimal::decimal_fast64_t) { return 16; }
constexpr int decimal_precision(decimal::decimal_fast128_t) { return 34; }

template <class Decimal>
struct decimal_readable_field_traits
{
    static constexpr bool is_supported = true;
    static BOOST_INLINE_CONSTEXPR const char* type_name = decimal_type_name(Decimal());
    static bool meta_check(meta_check_context& ctx)
    {
        return meta_check_decimal_impl(ctx, decimal_precision(Decimal()), type_name);
    }
    static error_code parse(field_view input, Decimal& output)
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
};

// clang-format off
template <> struct readable_field_traits<decimal::decimal32_t, void> : decimal_readable_field_traits<decimal::decimal32_t> {};
template <> struct readable_field_traits<decimal::decimal_fast32_t, void> : decimal_readable_field_traits<decimal::decimal_fast32_t> {};
template <> struct readable_field_traits<decimal::decimal64_t, void> : decimal_readable_field_traits<decimal::decimal64_t> {};
template <> struct readable_field_traits<decimal::decimal_fast64_t, void> : decimal_readable_field_traits<decimal::decimal_fast64_t> {};
template <> struct readable_field_traits<decimal::decimal128_t, void> : decimal_readable_field_traits<decimal::decimal128_t> {};
template <> struct readable_field_traits<decimal::decimal_fast128_t, void> : decimal_readable_field_traits<decimal::decimal_fast128_t> {};
// clang-format on

// format_sql support
template <class Decimal>
struct decimal_formatter
{
    const char* parse(const char* begin, const char*) { return begin; }
    void format(Decimal value, format_context_base& ctx) const
    {
        // MySQL's DECIMAL doesn't support NaN or Inf
        if (decimal::isnan(value) || decimal::isinf(value))
        {
            ctx.add_error(client_errc::unformattable_value);
            return;
        }

        // MySQL's DECIMAL uses fixed precision and a max precision of 65.
        // With sign and radix, that's 67 characters max.
        // Boost.Decimal can represent values that might yield longer representations
        // (as it uses floating point representations). Buffer overflows cause a value_too_large error
        char buffer[67]{};
        auto result = decimal::to_chars(buffer, buffer + sizeof(buffer), value, decimal::chars_format::fixed);
        if (result.ec != std::errc())
        {
            ctx.add_error(
                result.ec == std::errc::value_too_large ? error_code(client_errc::unformattable_value)
                                                        : error_code(std::make_error_code(result.ec))
            );
            return;
        }
        ctx.append_raw(runtime(string_view(buffer, result.ptr - buffer)));
    }
};

}  // namespace detail

// clang-format off
template <> struct formatter<decimal::decimal32_t> : detail::decimal_formatter<decimal::decimal32_t> {};
template <> struct formatter<decimal::decimal_fast32_t> : detail::decimal_formatter<decimal::decimal_fast32_t> {};
template <> struct formatter<decimal::decimal64_t> : detail::decimal_formatter<decimal::decimal64_t> {};
template <> struct formatter<decimal::decimal_fast64_t> : detail::decimal_formatter<decimal::decimal_fast64_t> {};
template <> struct formatter<decimal::decimal128_t> : detail::decimal_formatter<decimal::decimal128_t> {};
template <> struct formatter<decimal::decimal_fast128_t> : detail::decimal_formatter<decimal::decimal_fast128_t> {};
// clang-format on

}  // namespace mysql
}  // namespace boost

#endif

#endif
