//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_SNIPPETS_DESCRIBE_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_SNIPPETS_DESCRIBE_HPP

#include <boost/describe/class.hpp>

#include <string>

namespace boost {
namespace mysql {
namespace test {

//[describe_post
// We can use a plain struct with ints and strings to describe our rows.
// This must be placed at the namespace level
struct post
{
    int id;
    std::string title;
    std::string body;
};

// We must use Boost.Describe to add reflection capabilities to post.
// We must list all the fields that should be populated by Boost.MySQL
BOOST_DESCRIBE_STRUCT(post, (), (id, title, body))
//]

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
