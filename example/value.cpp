//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/value.hpp"
#include <iostream>

#define ASSERT(expr) \
    if (!(expr)) \
    { \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1); \
    }

void example_get()
{
    //[value_example_get
    boost::mysql::value v ("hello"); // v contains type boost::string_view
    boost::string_view typed_val = v.get<boost::string_view>(); // retrieves the underlying string
    ASSERT(typed_val == "hello");
    try
    {
        v.get<double>(); // wrong type! throws boost::mysql::bad_value_access
    }
    catch (const boost::mysql::bad_value_access&)
    {
    }
    //]
}

void example_get_optional()
{
    //[value_example_get_optional
    boost::mysql::value v (3.14); // v contains type double
    boost::optional<double> typed_val = v.get_optional<double>();
    ASSERT(typed_val.has_value()); // the optional is not empty
    ASSERT(*typed_val == 3.14); // and contains the right value

    boost::optional<boost::string_view> other =
        v.get_optional<boost::string_view>(); // wrong type!
    ASSERT(!other.has_value()); // empty optional
    //]
}

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
void example_get_std_optional()
{
    //[value_example_get_std_optional
    boost::mysql::value v (3.14); // v contains type double
    std::optional<double> typed_val = v.get_std_optional<double>();
    ASSERT(typed_val.has_value()); // the optional is not empty
    ASSERT(*typed_val == 3.14); // and contains the right value

    std::optional<boost::string_view> other =
        v.get_std_optional<boost::string_view>(); // wrong type!
    ASSERT(!other.has_value()); // empty optional
    //]
}
#endif

void example_get_conversions()
{
    //[value_example_get_conversions
    boost::mysql::value v (std::uint64_t(42)); // v contains type std::uint64_t
    ASSERT(v.get<std::uint64_t>() == 42u); // exact type match
    ASSERT(v.get<std::int64_t>() == 42); // converts from std::uint64_t -> std::int64_t
    //]
}

void example_inefficient()
{
    //[value_example_inefficient
    // WARNING!! Inefficient, do NOT do
    boost::mysql::value v (3.14); // get the value e.g. using query
    if (v.is_convertible_to<double>()) // it should be double, but we are not use
    {
        double val = v.get<double>();
        // Do stuff with val
        ASSERT(val == 3.14);
    }
    else
    {
        // oops, value didn't contain a double, handle the error
    }
    //]
}

void example_inefficient_ok()
{
    //[value_example_inefficient_ok
    boost::mysql::value v (3.14); // get the value e.g. using query
    boost::optional<double> val = v.get_optional<double>();
    // it should be double, but we are not use
    if (val)
    {
        // Do stuff with val
        ASSERT(*val == 3.14);
    }
    else
    {
        // oops, value didn't contain a double, handle the error
    }
    //]
}

void example_is()
{
    //[value_example_is
    boost::mysql::value v (std::uint64_t(42)); // v contains type std::uint64_t
    ASSERT(v.is<std::uint64_t>()); // exact type match
    ASSERT(!v.is<std::int64_t>()); // does not consider conversions
    ASSERT(v.is_convertible_to<std::uint64_t>()); // exact type match
    ASSERT(v.is_convertible_to<std::int64_t>()); // considers conversions
    //]
}

int main()
{
    example_get();
    example_get_optional();
    #ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    example_get_std_optional();
    #endif
    example_get_conversions();
    example_inefficient();
    example_inefficient_ok();
    example_is();
}

