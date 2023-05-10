//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/config.hpp>

#include <boost/describe/class.hpp>

// Verify that our macros agree with Describe's
#if defined(BOOST_MYSQL_CXX14) && !defined(BOOST_DESCRIBE_CXX14)
static_assert(false, "BOOST_MYSQL_CXX14 but !BOOST_DESCRIBE_CXX14");
#elif !defined(BOOST_MYSQL_CXX14) && defined(BOOST_DESCRIBE_CXXBAD)
static_assert(false, "!BOOST_MYSQL_CXX14 but BOOST_DESCRIBE_CXX14");
#endif
