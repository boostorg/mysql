//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_FIELD_TRAITS_BOOST_OPTIONAL_HPP
#define BOOST_MYSQL_DETAIL_TYPING_FIELD_TRAITS_BOOST_OPTIONAL_HPP

#include <boost/mysql/detail/typing/field_traits.hpp>

#include <boost/optional/optional.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class T>
struct field_traits<boost::optional<T>>
{
    static constexpr bool is_supported = field_traits<T>::is_supported;
    static constexpr const char* type_name = "boost::optional<T>";
    static void meta_check(meta_check_context& ctx)
    {
        ctx.set_cpp_type_name(field_traits<T>::type_name);
        ctx.set_nullability_checked(true);
        field_traits<T>::meta_check(ctx);
    }
    static error_code parse(field_view input, boost::optional<T>& output)
    {
        if (input.is_null())
        {
            output = boost::optional<T>{};
            return error_code();
        }
        else
        {
            output.emplace();
            return field_traits<T>::parse(input, *output);
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
