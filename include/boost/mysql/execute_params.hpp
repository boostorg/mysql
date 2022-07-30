//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXECUTE_PARAMS_HPP
#define BOOST_MYSQL_EXECUTE_PARAMS_HPP

#include <boost/mysql/value.hpp>
#include <boost/mysql/prepared_statement.hpp>
#include <boost/mysql/detail/auxiliar/stringize.hpp>
#include <boost/mysql/detail/auxiliar/value_type_traits.hpp>
#include <iterator>
#include <stdexcept>
#include <type_traits>

namespace boost {
namespace mysql {

// TODO: move this to impl file
namespace detail {

template <class ValueForwardIterator>
void check_num_params(
    ValueForwardIterator first,
    ValueForwardIterator last,
    const prepared_statement& stmt
)
{
    auto param_count = std::distance(first, last);
    if (param_count != stmt.num_params())
    {
        throw std::domain_error(detail::stringize(
                "prepared_statement::execute: expected ", stmt.num_params(), " params, but got ", param_count));
    }
}

} // detail


/**
  * \brief Represents the parameters required to execute a [reflink prepared_statement].
  * \details In essence, this class contains an iterator range \\[first, last), pointing
  * to a sequence of [reflink value]s that will be used as parameters to execute a
  * [reflink prepared_statement]. ValueForwardIterator must meet the [reflink ValueForwardIterator]
  * type requirements.
  *
  * In the future, this class may define extra members providing finer control
  * over prepared statement execution.
  * 
  * The \ref make_execute_params helper functions make it easier to create
  * instances of this class.
  */
template <class ValueForwardIterator>
class execute_params
{
    std::uint32_t statement_id_;
    ValueForwardIterator first_;
    ValueForwardIterator last_;
    static_assert(detail::is_value_forward_iterator<ValueForwardIterator>::value, 
        "ValueForwardIterator requirements not met");
public:
    /// Constructor.
    execute_params(
        const prepared_statement& stmt,
        ValueForwardIterator first,
        ValueForwardIterator last
    ) :
        statement_id_(stmt.id()),
        first_(first),
        last_(last)
    {
        detail::check_num_params(first, last, stmt);
    }

    constexpr std::uint32_t statement_id() const noexcept { return statement_id_; }

    /// Retrieves the parameter value range's begin. 
    constexpr ValueForwardIterator first() const { return first_; }

    /// Retrieves the parameter value range's end. 
    constexpr ValueForwardIterator last() const { return last_; }
};

template <
    class ValueForwardIterator,
    class EnableIf = detail::enable_if_value_forward_iterator<ValueForwardIterator>
>
constexpr execute_params<ValueForwardIterator>
make_execute_params(
    const prepared_statement& stmt,
    ValueForwardIterator first, 
    ValueForwardIterator last
)
{
    return execute_params<ValueForwardIterator>(stmt, first, last);
}

template <
    class ValueCollection,
    class EnableIf = detail::enable_if_value_collection<ValueCollection>
>
constexpr auto make_execute_params(
    const prepared_statement& stmt,
    const ValueCollection& col
) -> execute_params<decltype(std::begin(col))>
{
    return make_execute_params(stmt, std::begin(col), std::end(col));
}

} // mysql
} // boost


#endif
