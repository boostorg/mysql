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

/**
 * \brief (EXPERIMENTAL) An extension point to customize SQL formatting.
 * \details
 * This type can be specialized for custom types to make them formattable.
 * This makes them satisfy the `Formattable` concept, and usable in
 * \ref format_sql and \ref format_context_base::append_value.
 * \n
 * A `formatter` specialization for a type `T` should have the following form:
 * ```
 * template <> struct formatter<MyType> {
 *     static void format(const MyType&, format_context_base&);
 * }
 * ```
 * \n
 * Don't specialize `formatter` for built-in types, like `int`, `std::string` or
 * optionals (formally, any type satisfying `WritableField`). This is not supported
 * and will result in a compile-time error.
 */
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
 * Conceptually, a format context contains: \n
 *   \li The result string. Output operations append characters to this output string.
 *       `format_context_base` is agnostic to the output string type.
 *   \li \ref format_options required to format values.
 *   \li An error state (\ref error_state) that is set by output operations when they fail.
 *       The error state is propagated to \ref basic_format_context::get.
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

public:
    /**
     * \brief Adds raw SQL to the output string.
     * \details
     * Adds raw, unescaped SQL to the output string. Doesn't alter the error state.
     * \n
     * By default, the passed SQL should be available at compile-time.
     * Use \ref runtime if you need to use runtime values.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations may throw.
     *
     * \par Object lifetimes
     * The passed string is copied as required and doesn't need to be kept alive.
     */
    format_context_base& append_raw(constant_string_view sql)
    {
        impl_.output.append(sql.get());
        return *this;
    }

    /**
     * \brief Formats a value and adds it to the output string.
     * \details
     * `value` is formatted according to its type. If formatting
     * generates an error (for instance, a string with invalid encoding is passed),
     * the error state may be set.
     * \n
     * The supplied type must satisfy the `Formattable` concept.
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations may throw.
     *
     * \par Errors
     * The error state may be updated with the following errors:
     * \li \ref client_errc::invalid_encoding if a string with an invalid encoding is passed.
     * \li \ref client_errc::unformattable_value if a NaN or infinity `float` or `double` is passed.
     * \li Any other error code that user-supplied formatter specializations may add using \ref add_error.
     */
    template <BOOST_MYSQL_FORMATTABLE Formattable>
    format_context_base& append_value(const Formattable& v)
    {
        format_arg(detail::make_format_value(v));
        return *this;
    }

    /**
     * \brief Adds an error to the current error state.
     * \details
     * This function can be used by custom formatters to report that they
     * received a value that can't be formatted. For instance, it's used by
     * the built-in string formatter when a string with an invalid encoding is supplied.
     * \n
     * If the error state is not set before calling this function, the error
     * state is updated to `ec`. Otherwise, the error is ignored.
     * This implies that once the error state is set, it can't be reset.
     * \n
     * \par Exception safety
     * No-throw guarantee.
     */
    void add_error(error_code ec) noexcept
    {
        if (!impl_.ec)
            impl_.ec = ec;
    }

    /**
     * \brief Retrieves the current error state.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    error_code error_state() const noexcept { return impl_.ec; }

    /**
     * \brief Retrieves the format options.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    format_options format_opts() const noexcept { return impl_.opts; }
};

/**
 * \brief (EXPERIMENTAL) Implements stream-like SQL formatting.
 * \details
 * The primary interface for stream-like SQL formatting. Contrary to \ref format_context_base,
 * this type is aware of the output string's actual type. `basic_format_context` owns
 * an instance of `OutputString`. Format operations will append characters to such string.
 * \n
 * `basic_format_context` are single-use: once the result has been retrieved using \ref get,
 * they cannot be re-used. This is a move-only type.
 * \see format_context
 */
template <BOOST_MYSQL_OUTPUT_STRING OutputString>
class basic_format_context : public format_context_base
{
    OutputString output_{};

    detail::output_string_ref ref() noexcept { return detail::output_string_ref::create(output_); }

public:
    /**
     * \brief Constructor.
     * \details
     * Uses a default-constructed `OutputString` as output string, and an empty
     * error code as error state. This constructor can only be called if `OutputString`
     * is default-constructible.
     *
     * \par Exception safety
     * Strong guarantee: exceptions thrown by default-constructing `OutputString` are propagated.
     */
    explicit basic_format_context(format_options opts
    ) noexcept(std::is_nothrow_default_constructible<OutputString>::value)
        : format_context_base(ref(), opts)
    {
    }

    /**
     * \brief Constructor.
     * \details
     * Move constructs an `OutputString` using `storage`. After construction,
     * the output string is cleared. Uses an empty
     * error code as error state. This constructor allows re-using existing
     * memory for the output string.
     * \n
     *
     * \par Exception safety
     * Basic guarantee: exceptions thrown by move-constructing `OutputString` are propagated.
     */
    basic_format_context(format_options opts, OutputString&& storage) noexcept(
        std::is_nothrow_move_constructible<OutputString>::value
    )
        : format_context_base(ref(), opts), output_(std::move(storage))
    {
        output_.clear();
    }

    basic_format_context(const basic_format_context&) = delete;
    basic_format_context& operator=(const basic_format_context&) = delete;

    /**
     * \brief Move constructor.
     * \details
     * Move constructs an `OutputString` using `rhs`'s output string.
     * `*this` will have the same format options and error state than `rhs`.
     * `rhs` is left in a valid but unspecified state.
     *
     * \par Exception safety
     * Basic guarantee: exceptions thrown by move-constructing `OutputString` are propagated.
     */
    basic_format_context(basic_format_context&& rhs
    ) noexcept(std::is_nothrow_move_constructible<OutputString>::value)
        : format_context_base(ref(), rhs), output_(std::move(rhs.output_))
    {
    }

    /**
     * \brief Move assignment.
     * \details
     * Move assigns `rhs`'s output string to `*this` output string.
     * `*this` will have the same format options and error state than `rhs`.
     * `rhs` is left in a valid but unspecified state.
     *
     * \par Exception safety
     * Basic guarantee: exceptions thrown by move-constructing `OutputString` are propagated.
     */
    basic_format_context& operator=(basic_format_context&& rhs
    ) noexcept(std::is_nothrow_move_assignable<OutputString>::value)
    {
        output_ = std::move(rhs.output_);
        assign(rhs);
        return *this;
    }

    /**
     * \brief Retrieves the result of the formatting operation.
     * \details
     * After running the relevant formatting operations (using \ref append_raw,
     * \ref append_value or \ref format_sql_to), call this function to retrieve the
     * overall result of the operation.
     * \n
     * If \ref error_state is a non-empty error code, returns it as an error.
     * Otherwise, returns the output string, move-constructing it into the `system::result` object.
     * \n
     * This function is move-only: once called, `*this` is left in a valid but unspecified state.
     *
     * \par Exception safety
     * Basic guarantee: exceptions thrown by move-constructing `OutputString` are propagated.
     */
    system::result<OutputString> get() && noexcept(std::is_nothrow_move_constructible<OutputString>::value)
    {
        auto ec = error_state();
        if (ec)
            return ec;
        return std::move(output_);
    }
};

/**
 * \brief (EXPERIMENTAL) Implements stream-like SQL formatting.
 * \details
 * Convenience type alias for `basic_format_context`'s most common case.
 */
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
    detail::format_arg
#endif
    arg(string_view name, const T& value) noexcept
{
    return {detail::make_format_value(value), name};
}

template <BOOST_MYSQL_FORMATTABLE... Formattable>
void format_sql_to(constant_string_view format_str, format_context_base& ctx, const Formattable&... args)
{
    detail::format_arg_store<sizeof...(Formattable)> store(args...);
    detail::vformat_sql_to(format_str.get(), ctx, store.get());
}

/// (EXPERIMENTAL) Composes a SQL query client-side (TODO).
template <BOOST_MYSQL_FORMATTABLE... Formattable>
std::string format_sql(
    constant_string_view format_str,
    const format_options& opts,
    const Formattable&... args
)
{
    format_context ctx(opts);
    format_sql_to(format_str, ctx, args...);
    return detail::check_format_result(std::move(ctx).get());
}

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/format_sql.ipp>
#endif

#endif
