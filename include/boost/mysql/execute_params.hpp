//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXECUTE_PARAMS_HPP
#define BOOST_MYSQL_EXECUTE_PARAMS_HPP

#include <boost/mysql/value.hpp>
#include <boost/mysql/detail/auxiliar/value_type_traits.hpp>
#include <iterator>
#include <type_traits>

namespace boost {
namespace mysql {

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
    ValueForwardIterator first_;
    ValueForwardIterator last_;
    static_assert(detail::is_value_forward_iterator<ValueForwardIterator>::value, 
        "ValueForwardIterator requirements not met");
public:
    /// Constructor.
    constexpr execute_params(ValueForwardIterator first, ValueForwardIterator last) :
        first_(first), last_(last) {}

    /// Retrieves the parameter value range's begin. 
    constexpr ValueForwardIterator first() const { return first_; }

    /// Retrieves the parameter value range's end. 
    constexpr ValueForwardIterator last() const { return last_; }

    /// Sets the parameter value range's begin.
    void set_first(ValueForwardIterator v) { first_ = v;}

    /// Sets the parameter value range's end.
    void set_last(ValueForwardIterator v) { last_ = v; }
};

/**
  * \relates execute_params
  * \brief Creates an instance of [reflink execute_params] from a pair of iterators.
  * \details ValueForwardIterator should meet the [reflink ValueForwardIterator] type requirements.
  */
template <
    class ValueForwardIterator,
    class EnableIf = detail::enable_if_value_forward_iterator<ValueForwardIterator>
>
constexpr execute_params<ValueForwardIterator>
make_execute_params(
    ValueForwardIterator first, 
    ValueForwardIterator last
)
{
    return execute_params<ValueForwardIterator>(first, last);
}

/**
  * \relates execute_params
  * \brief Creates an instance of [reflink execute_params] from a collection.
  * \details ValueCollection should meet the [reflink ValueCollection] type requirements.
  */
template <
    class ValueCollection,
    class EnableIf = detail::enable_if_value_collection<ValueCollection>
>
constexpr auto make_execute_params(
    const ValueCollection& col
) -> execute_params<decltype(std::begin(col))>
{
    return make_execute_params(std::begin(col), std::end(col));
}

}
}


#endif
