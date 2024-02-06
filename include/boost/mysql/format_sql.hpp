//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FORMAT_SQL_HPP
#define BOOST_MYSQL_FORMAT_SQL_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/constant_string_view.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/format_sql.hpp>
#include <boost/mysql/detail/output_string.hpp>

#include <boost/config.hpp>
#include <boost/core/span.hpp>
#include <boost/system/result.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>

#ifdef BOOST_MYSQL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace mysql {

/**
 * \brief (EXPERIMENTAL) A SQL identifier to use for client-side SQL formatting.
 * \details
 * Represents a possibly-qualified SQL identifier.
 *
 * \par Object lifetimes
 * This type is non-owning, and should only be used as an argument to SQL formatting
 * functions.
 */
class identifier
{
    struct impl_t
    {
        std::size_t qual_level;
        string_view ids[3];

        BOOST_CXX14_CONSTEXPR impl_t(
            std::size_t qual_level,
            string_view id1,
            string_view id2,
            string_view id3
        ) noexcept
            : qual_level(qual_level), ids{id1, id2, id3}
        {
        }
    } impl_;

    friend struct detail::access;

public:
    /**
     * \brief Constructs an unqualified identifier.
     * \details
     * Unqualified identifiers are usually field, table or database names,
     * and get formatted as `"`column_name`"`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    BOOST_CXX14_CONSTEXPR explicit identifier(string_view id) noexcept : impl_(1u, id, {}, {}) {}

    /**
     * \brief Constructs an identifier with a single qualifier.
     * \details
     * Identifiers with one qualifier are used for field, table and view names.
     * The qualifier identifies the parent object. For instance,
     * `qualifier("table_name", "field_name")` maps to `"`table_name`.`field_name`"`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    BOOST_CXX14_CONSTEXPR identifier(string_view qualifier, string_view id) noexcept
        : impl_(2u, qualifier, id, {})
    {
    }

    /**
     * \brief Constructs an identifier with two qualifiers.
     * \details
     * Identifiers with two qualifier are used for field names.
     * The first qualifier identifies the database, the second, the table name.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    BOOST_CXX14_CONSTEXPR identifier(string_view qual1, string_view qual2, string_view id) noexcept
        : impl_(3u, qual1, qual2, id)
    {
    }
};

/// (EXPERIMENTAL) An extension point to customize SQL formatting (TODO).
template <class T>
struct formatter
#ifndef BOOST_MYSQL_DOXYGEN
    : detail::formatter_is_unspecialized
{
}
#endif
;

/**
 * \brief (EXPERIMENTAL) Base class for stream-oriented SQL formatting.
 * \details
 * A base class for stream-oriented SQL formatting. This class can't be
 * instantiated directly - use \ref basic_format_context, instead.
 * Do not subclass it, either.
 * \n
 * References to this class are useful when you need to manipulate
 * a format context without knowing the type of the actual context that will be used,
 * like when specializing \ref formatter.
 */
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
    /// TODO: document
    format_context_base& append_raw(constant_string_view sql)
    {
        impl_.output.append(sql.get());
        return *this;
    }

    /// TODO: document
    template <BOOST_MYSQL_FORMATTABLE T>
    format_context_base& append_value(const T& v)
    {
        format_arg(detail::make_format_value(v));
        return *this;
    }
};

/// (EXPERIMENTAL) Implements stream-like SQL formatting (TODO).
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
        output_.clear();
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

/// (EXPERIMENTAL) Implements stream-like SQL formatting (TODO).
using format_context = basic_format_context<std::string>;

template <>
struct formatter<identifier>
{
    BOOST_MYSQL_DECL
    static void format(const identifier& value, format_context_base& ctx);
};

/// (EXPERIMENTAL) Creates a named argument for SQL formatting (TODO).
template <class T>
inline
#ifdef BOOST_MYSQL_DOXYGEN
    __see_below__
#else
    detail::format_arg_descriptor
#endif
    arg(string_view name, const T& value) noexcept
{
    return {detail::make_format_value(value), name};
}

template <BOOST_MYSQL_FORMATTABLE... Args>
void format_sql_to(constant_string_view format_str, format_context_base& ctx, const Args&... args)
{
    detail::format_arg_store<sizeof...(Args)> store(args...);
    detail::vformat_sql_to(format_str.get(), ctx, store.get());
}

/// (EXPERIMENTAL) Composes a SQL query client-side (TODO).
template <BOOST_MYSQL_FORMATTABLE... Args>
std::string format_sql(constant_string_view format_str, const format_options& opts, const Args&... args)
{
    format_context ctx(opts);
    format_sql_to(format_str, ctx, args...);
    return detail::check_format_result(ctx.get());
}

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/format_sql.ipp>
#endif

#endif
