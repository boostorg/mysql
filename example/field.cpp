//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/field_view.hpp>

#include <iostream>

#define ASSERT(expr)                                          \
    if (!(expr))                                              \
    {                                                         \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1);                                              \
    }

void example_as()
{
    //[value_example_get
    boost::mysql::field_view v("hello");           // v contains type boost::string_view
    boost::string_view typed_val = v.as_string();  // retrieves the underlying string
    ASSERT(typed_val == "hello");
    try
    {
        v.as_double();  // wrong type! throws boost::mysql::bad_field_access
        // v.get_double(); // don't do this - UB
    }
    catch (const boost::mysql::bad_field_access&)
    {
    }
    //]
}

void example_is()
{
    //[value_example_is
    boost::mysql::field_view v(std::uint64_t(42));  // v contains type std::uint64_t
    ASSERT(v.is_uint64());                          // exact type match
    ASSERT(!v.is_int64());                          // the underlying type is unsigned
    ASSERT(!v.is_string());
    //]
}

int main()
{
    example_as();
    example_is();
}
