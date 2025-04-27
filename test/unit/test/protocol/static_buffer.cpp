//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/protocol/static_buffer.hpp>

#include <boost/test/unit_test.hpp>

#include <array>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"

using namespace boost::mysql::test;
using boost::mysql::detail::static_buffer;

namespace {

BOOST_AUTO_TEST_SUITE(test_static_buffer)

// Constructors
BOOST_AUTO_TEST_CASE(default_constructor)
{
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(static_buffer<32>(), std::vector<std::uint8_t>());
}

BOOST_AUTO_TEST_CASE(init_constructor)
{
    // Zero size
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(static_buffer<32>(0), std::vector<std::uint8_t>());

    // Intermediate size
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(static_buffer<32>(10), std::vector<std::uint8_t>(10, 0x00));

    // Max size
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(static_buffer<32>(32), std::vector<std::uint8_t>(32, 0x00));
}

// Accessors
BOOST_AUTO_TEST_CASE(data_size_const)
{
    const static_buffer<32> buff(8);
    BOOST_TEST(buff.data() != nullptr);
    BOOST_TEST(buff.size() == 8u);
}

BOOST_AUTO_TEST_CASE(data_size_nonconst)
{
    static_buffer<32> buff(8);
    BOOST_TEST(buff.data() != nullptr);
    BOOST_TEST(buff.size() == 8u);
}

// clear
BOOST_AUTO_TEST_CASE(clear_empty)
{
    static_buffer<32> v;
    v.clear();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(v, std::vector<std::uint8_t>());
}

BOOST_AUTO_TEST_CASE(clear_not_empty)
{
    static_buffer<32> v;
    v.append(std::array<std::uint8_t, 5>{
        {0, 1, 2, 3, 4}
    });
    BOOST_TEST(v.size() == 5u);
    v.clear();
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(v, std::vector<std::uint8_t>());
}

// append
BOOST_AUTO_TEST_CASE(append_from_empty_to_empty)
{
    static_buffer<32> v;
    v.append({});
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(v, std::vector<std::uint8_t>());
}

BOOST_AUTO_TEST_CASE(append_from_empty_to_midsize)
{
    const std::array<std::uint8_t, 3> data{
        {1, 2, 3}
    };
    static_buffer<32> v;
    v.append(std::vector<std::uint8_t>(data.begin(), data.end()));  // verify we make a copy
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(v, data);
}

BOOST_AUTO_TEST_CASE(append_from_empty_to_maxsize)
{
    const std::vector<std::uint8_t> data(32, 0xde);
    static_buffer<32> v;
    v.append(data);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(v, data);
}

BOOST_AUTO_TEST_CASE(append_from_midsize_to_midsize)
{
    // Initial
    static_buffer<32> v;
    v.append(std::vector<std::uint8_t>{2, 2, 2});

    // Append more data
    v.append(std::vector<std::uint8_t>{1, 2, 3});

    // Verify
    const std::vector<std::uint8_t> expected{2, 2, 2, 1, 2, 3};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(v, expected);
}

BOOST_AUTO_TEST_CASE(append_from_midsize_to_maxsize)
{
    // Initial
    static_buffer<32> v;
    v.append(std::vector<std::uint8_t>{1, 2, 3});

    // Append
    v.append(std::vector<std::uint8_t>(29, 0xde));

    // Verify
    const std::array<std::uint8_t, 32> expected{
        {0x01, 0x02, 0x03, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde,
         0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde}
    };
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(v, expected);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
