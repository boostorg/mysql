//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_GET_ROW_TYPE_HPP
#define BOOST_MYSQL_GET_ROW_TYPE_HPP

namespace boost {
namespace mysql {

// TODO: do we want this header?
// TODO: document

template <class RowType>
struct get_row_type
{
    using type = RowType;
};

template <class RowType>
using get_row_type_t = typename get_row_type<RowType>::type;

}  // namespace mysql
}  // namespace boost

#endif
