//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONSTANT_STRING_VIEW_HPP
#define BOOST_MYSQL_CONSTANT_STRING_VIEW_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>

#include <boost/config.hpp>

#include <type_traits>

namespace boost {
namespace mysql {

/// (EXPERIMENTAL) A string_view for values that should be known at compile-time (TODO).
class constant_string_view
{
    string_view impl_;

    BOOST_CXX14_CONSTEXPR constant_string_view(string_view value, int) noexcept : impl_(value) {}
    friend BOOST_CXX14_CONSTEXPR inline constant_string_view runtime(string_view) noexcept;

public:
    template <
        class T,
        class = typename std::enable_if<std::is_convertible<const T&, string_view>::value>::type>
    BOOST_MYSQL_CONSTEVAL constant_string_view(const T& value) noexcept : impl_(value)
    {
    }

    BOOST_CXX14_CONSTEXPR string_view get() const noexcept { return impl_; }
};

/// (EXPERIMENTAL) Creates a \ref constant_string_view from a runtime value (TODO).
BOOST_CXX14_CONSTEXPR inline constant_string_view runtime(string_view value) noexcept
{
    return constant_string_view(value, 0);
}

}  // namespace mysql
}  // namespace boost

#endif
