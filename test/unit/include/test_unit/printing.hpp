//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_PRINTING_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_PRINTING_HPP

#include <boost/mysql/detail/results_iterator.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>
#include <boost/mysql/detail/rows_iterator.hpp>

#include <boost/test/unit_test.hpp>

#include <ostream>

namespace boost {
namespace mysql {
namespace detail {

// template <std::size_t max_size>
// std::ostream& operator<<(std::ostream& os, const static_string<max_size>& value)
// {
//     return os << value.value();
// }

inline std::ostream& operator<<(std::ostream& os, resultset_encoding t)
{
    switch (t)
    {
    case resultset_encoding::binary: return os << "binary";
    case resultset_encoding::text: return os << "text";
    default: return os << "unknown";
    }
}

inline std::ostream& operator<<(std::ostream& os, results_iterator it)
{
    return os << "results_iterator(" << static_cast<const void*>(it.obj()) << ", index=" << it.index() << ")";
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::time)
BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::mysql::detail::rows_iterator)

#endif
