//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <algorithm>
#include <boost/asio/buffer.hpp>
#include <boost/mysql/detail/channel/read_buffer.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

using boost::mysql::detail::read_buffer;
using boost::asio::const_buffer;
using boost::asio::mutable_buffer;

static bool equal(const_buffer lhs, const_buffer rhs)
{
    return lhs.data() == rhs.data() && lhs.size() == rhs.size();
}

static bool equal(mutable_buffer lhs, mutable_buffer rhs)
{
    return lhs.data() == rhs.data() && lhs.size() == rhs.size();
}

static bool equal(const_buffer lhs, const_buffer rhs, const std::vector<std::uint8_t>& bytes)
{
    return equal(lhs, rhs) && 
        lhs.size() == bytes.size() &&
        std::equal((const std::uint8_t*)lhs.data(), (const std::uint8_t*)lhs.data() + lhs.size(), bytes.begin());
}

static void copy(read_buffer& buff, const std::vector<std::uint8_t>& bytes)
{
    std::copy(bytes.begin(), bytes.end(), buff.free_first());
}

// move to current message 0 bytes
// move to current message partial
// move to current message all bytes
// remove current message last 0 bytes
// remove current message partial
// remove current message total
// remove current message with pending
// remove current message without pending
// remove current message with free (?)
// remove current message without free (?)
// move to reserved 0 bytes
// move to reserved partial
// move to reserved total
// remove reserved without reserved, without other areas
// remove reserved without reserved, with other areas
// remove reserved with reserved, without other areas
// remove reserved with reserved, with other areas
// grow to fit with 0 request
// grow to fit but there is enough space
// grow to fit with other areas

BOOST_AUTO_TEST_SUITE(test_read_buffer)

BOOST_AUTO_TEST_SUITE(init_ctor)

BOOST_AUTO_TEST_CASE(some_initial_size)
{
    read_buffer buff (531);
    auto first = buff.reserved_first();
    
    BOOST_TEST(first != nullptr);
    BOOST_TEST(buff.free_size() == buff.total_size());
    BOOST_TEST(buff.total_size() >= 531);
    BOOST_TEST(equal(buff.reserved_area(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.current_message(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.pending_area(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.free_area(), mutable_buffer(first, buff.free_size())));
}

BOOST_AUTO_TEST_CASE(zero_initial_size)
{
    read_buffer buff (0);

    BOOST_TEST(buff.total_size() == 0);
    BOOST_TEST(equal(buff.reserved_area(), const_buffer()));
    BOOST_TEST(equal(buff.current_message(), const_buffer()));
    BOOST_TEST(equal(buff.pending_area(), const_buffer()));
    BOOST_TEST(equal(buff.free_area(), mutable_buffer()));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(move_to_pending)

BOOST_AUTO_TEST_CASE(some_bytes)
{
    read_buffer buff (512);
    std::vector<std::uint8_t> contents { 0x01, 0x02, 0x03, 0x04 };
    copy(buff, contents);
    buff.move_to_pending(4);
    auto first = buff.reserved_first();

    BOOST_TEST(equal(buff.reserved_area(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.current_message(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.pending_area(), const_buffer(first, 4), contents));
    BOOST_TEST(equal(buff.free_area(), const_buffer(first + 4, buff.total_size() - 4)));
}

BOOST_AUTO_TEST_CASE(all_bytes)
{
    read_buffer buff (8);
    std::vector<std::uint8_t> contents (buff.total_size(), 0x01);
    copy(buff, contents);
    buff.move_to_pending(buff.total_size());
    auto first = buff.reserved_first();

    BOOST_TEST(equal(buff.reserved_area(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.current_message(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.pending_area(), const_buffer(first, buff.total_size()), contents));
    BOOST_TEST(equal(buff.free_area(), const_buffer(first + buff.total_size(), 0)));
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    read_buffer buff (8);
    buff.move_to_pending(0);
    auto first = buff.reserved_first();

    BOOST_TEST(equal(buff.reserved_area(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.current_message(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.pending_area(), const_buffer(first, 0)));
    BOOST_TEST(equal(buff.free_area(), const_buffer(first, buff.total_size())));
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE_END()
