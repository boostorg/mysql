//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXECUTE_PARAMS_HPP
#define BOOST_MYSQL_EXECUTE_PARAMS_HPP

#include <boost/mysql/prepared_statement.hpp>
#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <iterator>
#include <type_traits>

namespace boost {
namespace mysql {

/**
  * \brief Represents the parameters required to execute a [reflink prepared_statement].
  * \details In essence, this class contains an iterator range \\[first, last), pointing
  * to a sequence of [reflink value]s that will be used as parameters to execute a
  * [reflink prepared_statement]. FieldViewFwdIterator must meet the [reflink FieldViewFwdIterator]
  * type requirements.
  *
  * In the future, this class may define extra members providing finer control
  * over prepared statement execution.
  * 
  * The \ref make_execute_params helper functions make it easier to create
  * instances of this class.
  */
template <class FieldViewFwdIterator>
class execute_params
{
    std::uint32_t statement_id_;
    FieldViewFwdIterator first_;
    FieldViewFwdIterator last_;
    static_assert(detail::is_field_view_forward_iterator<FieldViewFwdIterator>::value, 
        "FieldViewFwdIterator requirements not met");
public:
    /// Constructor.
    execute_params(
        const prepared_statement& stmt,
        FieldViewFwdIterator first,
        FieldViewFwdIterator last
    );

    constexpr std::uint32_t statement_id() const noexcept { return statement_id_; }

    /// Retrieves the parameter value range's begin. 
    constexpr FieldViewFwdIterator first() const { return first_; }

    /// Retrieves the parameter value range's end. 
    constexpr FieldViewFwdIterator last() const { return last_; }
};

template <
    class FieldViewFwdIterator,
    class EnableIf = detail::enable_if_field_view_forward_iterator<FieldViewFwdIterator>
>
constexpr execute_params<FieldViewFwdIterator>
make_execute_params(
    const prepared_statement& stmt,
    FieldViewFwdIterator first, 
    FieldViewFwdIterator last
)
{
    return execute_params<FieldViewFwdIterator>(stmt, first, last);
}

template <
    class FieldViewCollection,
    class EnableIf = detail::enable_if_field_view_collection<FieldViewCollection>
>
constexpr auto make_execute_params(
    const prepared_statement& stmt,
    const FieldViewCollection& col
) -> execute_params<decltype(std::begin(col))>
{
    return make_execute_params(stmt, std::begin(col), std::end(col));
}

} // mysql
} // boost

#include <boost/mysql/impl/execute_params.hpp>

#endif
