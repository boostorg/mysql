//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_STRINGIZE_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_STRINGIZE_HPP

#include <string>
#include <sstream>

namespace boost {
namespace mysql {
namespace detail {

inline void stringize_helper(std::ostream&) noexcept {}

template <class T, class... Types>
void stringize_helper(std::ostream& os, const T& input, const Types&... tail)
{
    os << input;
    stringize_helper(os, tail...);
}

template <class... Types>
std::string stringize(const Types&... inputs)
{
    std::ostringstream ss;
    stringize_helper(ss, inputs...);
    return ss.str();
}

} // detail
} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_STRINGIZE_HPP_ */
