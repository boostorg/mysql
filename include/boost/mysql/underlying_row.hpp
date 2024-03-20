//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_UNDERLYING_ROW_HPP
#define BOOST_MYSQL_UNDERLYING_ROW_HPP

#include <boost/mysql/detail/typing/row_traits.hpp>

namespace boost {
namespace mysql {

// TODO: do we want this header?
// TODO: document

template <class StaticRow>
using underlying_row_t = detail::underlying_row_t<StaticRow>;

}  // namespace mysql
}  // namespace boost

#endif
