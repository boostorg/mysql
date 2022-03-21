//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_CONTAINER_EQUALS_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_CONTAINER_EQUALS_HPP

#include <algorithm>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

template <class TLeft, class TRight>
inline bool container_equals(
    const std::vector<TLeft>& lhs,
    const std::vector<TRight>& rhs
)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    return std::equal(
        lhs.begin(),
        lhs.end(),
        rhs.begin()
    );
}

} // detail
} // mysql
} // boost



#endif
