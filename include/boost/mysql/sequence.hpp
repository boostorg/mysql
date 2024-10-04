//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SEQUENCE_HPP
#define BOOST_MYSQL_SEQUENCE_HPP

#include <boost/mysql/detail/sequence.hpp>

#ifdef BOOST_MYSQL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace mysql {

/**
 * \brief (EXPERIMENTAL) The return type of \ref sequence.
 * \details
 * Contains a range, a formatter function, and a glue string.
 * This type satisfies the `Formattable` concept. See \ref sequence for a detailed
 * description of what formatting this class does.
 * \n
 * Don't instantiate this class directly - use \ref sequence, instead.
 * The exact definition may vary between releases.
 */
template <class Range, class FormatFn>
struct format_sequence
#ifndef BOOST_MYSQL_DOXYGEN
{
    Range range;
    FormatFn fn;
    constant_string_view glue;
}
#endif
;

// TODO: document
template <class T>
using sequence_range_t = typename detail::sequence_range_type<T>::type;

/**
 * \brief Makes a range formattable by supplying a per-element formatter function.
 * \details
 * Objects returned by this function satisfy `Formattable`.
 * When formatted, the formatter function `fn` is invoked for each element
 * in the range. The glue string `glue` is output raw (as per \ref format_context_base::append_raw)
 * between consecutive invocations of the formatter function, generating an effect
 * similar to `std::ranges::views::join`.
 *
 * Range is decay-copied into the resulting object. This behavior can be disabled
 * by passing `std::reference_wrapper` objects, which are translated into references
 * (as happens in `std::make_tuple`). C arrays are translated into `std::array` objects.
 *
 * \par Type requirements
 *   - FormatFn should be move constructible.
 *   - Expressions `std::begin(range)` and `std::end(range)` should return an input iterator/sentinel
 *     pair that can be compared for (in)equality.
 *   - The expression `static_cast<const FormatFn&>(fn)(* std::begin(range), ctx)`
 *     should be well formed, with `ctx` begin a `format_context_base&`.
 *   - If `Range` is an instantiation of `std::reference_wrapper< U >`, no requirements are placed on `U`.
 *   - If `Range` is a lvalue C array, its elements should be copy-constructible.
 *   - If `Range` is a rvalue C array, its elements should be move-constructible.
 *   - Otherwise, if `Range` is an lvalue, `Range` should be copy-constructible.
 *   - Otherwise, if `Range` is an rvalue, `Range` should be copy-constructible.
 *
 * \par Exception safety
 * Basic guarantee. Propagates any exception that constructing the resulting
 * range or `FormatFn` may throw.
 */
template <class Range, class FormatFn>
#if defined(BOOST_MYSQL_HAS_CONCEPTS)
    requires std::move_constructible<FormatFn> &&
             detail::format_fn_for_range<FormatFn, sequence_range_t<Range>>
#endif
format_sequence<sequence_range_t<Range>, FormatFn> sequence(
    Range&& range,
    FormatFn fn,
    constant_string_view glue = ", "
)

{
    return {detail::cast_range(std::forward<Range>(range)), std::move(fn), glue};
}

template <class Range, class FormatFn>
struct formatter<format_sequence<Range, FormatFn>>
{
    const char* parse(const char* begin, const char*) { return begin; }

    void format(format_sequence<Range, FormatFn>& value, format_context_base& ctx) const
    {
        detail::do_format_sequence(value.range, value.fn, value.glue, ctx);
    }

    void format(const format_sequence<Range, FormatFn>& value, format_context_base& ctx) const
    {
        detail::do_format_sequence(value.range, value.fn, value.glue, ctx);
    }
};

}  // namespace mysql
}  // namespace boost

#endif
