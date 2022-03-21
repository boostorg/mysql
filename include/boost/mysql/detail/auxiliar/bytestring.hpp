//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_BYTESTRING_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_BYTESTRING_HPP

#include <vector>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

template <class Allocator>
using basic_bytestring = std::vector<std::uint8_t, Allocator>;

using bytestring = std::vector<std::uint8_t>;

}
}
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_BYTESTRING_HPP_ */
