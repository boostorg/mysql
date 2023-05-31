//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_EXECUTION_REQUEST_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_EXECUTION_REQUEST_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>

#include <boost/core/span.hpp>

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

template <class T>
struct is_bound_statement_tuple : std::false_type
{
};

template <class T>
struct is_bound_statement_tuple<bound_statement_tuple<T>> : std::true_type
{
};

template <class T>
struct is_bound_statement_range : std::false_type
{
};

template <class T>
struct is_bound_statement_range<bound_statement_iterator_range<T>> : std::true_type
{
};

template <class T>
struct is_execution_request
{
    using without_cvref = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
    static constexpr bool value = std::is_convertible<T, string_view>::value ||
                                  is_bound_statement_tuple<without_cvref>::value ||
                                  is_bound_statement_range<without_cvref>::value;
};

#ifdef BOOST_MYSQL_HAS_CONCEPTS

template <class T>
concept execution_request = is_execution_request<T>::value;

#define BOOST_MYSQL_EXECUTION_REQUEST ::boost::mysql::detail::execution_request

#else  // BOOST_MYSQL_HAS_CONCEPTS

#define BOOST_MYSQL_EXECUTION_REQUEST class

#endif  // BOOST_MYSQL_HAS_CONCEPTS

struct any_execution_request
{
    union data_t
    {
        string_view query;
        struct
        {
            statement stmt;
            span<const field_view> params;
        } stmt;

        data_t(string_view q) noexcept : query(q) {}
        data_t(statement s, span<const field_view> params) noexcept : stmt{s, params} {}
    } data;
    bool is_query;

    any_execution_request(string_view q) noexcept : data(q), is_query(true) {}
    any_execution_request(statement s, span<const field_view> params) noexcept
        : data(s, params), is_query(false)
    {
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
