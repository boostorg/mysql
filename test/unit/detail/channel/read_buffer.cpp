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
#include <ostream>
#include <vector>

using boost::mysql::detail::read_buffer;
using boost::asio::const_buffer;
using boost::asio::mutable_buffer;
using boost::asio::buffer;

namespace
{

struct buffer_printer
{
    const_buffer buff;
};

std::ostream& operator<<(std::ostream& os, buffer_printer buff)
{
    os << "{ ";
    for (std::size_t i = 0; i < buff.buff.size(); ++i)
    {
        os << static_cast<int>(static_cast<const std::uint8_t*>(buff.buff.data())[i]) << ", ";
    }
    return os << "}";
}

} // anon namespace

#define BOOST_MYSQL_BUFF_TEST(b1, b2) \
    BOOST_TEST( \
        std::memcmp(b1.data(), b2.data(), b1.size()) == 0, \
        #b1 " != " #b2 ": " << buffer_printer{b1} << " != " << buffer_printer{b2})

static void check_equal(const_buffer lhs, const_buffer rhs, const char* msg = "")
{
    BOOST_TEST(lhs.data() == rhs.data(), msg << ": " <<  lhs.data() << " != " << rhs.data());
    BOOST_TEST(lhs.size() == rhs.size(), msg << ": " << lhs.size() << " != " << rhs.size());
}

static void check_buffer(
    read_buffer& buff,
    const std::vector<std::uint8_t>& reserved,
    const std::vector<std::uint8_t>& current_message,
    const std::vector<std::uint8_t>& pending
)
{
    std::size_t current_message_offset = reserved.size();
    std::size_t pending_offset = current_message_offset + current_message.size();
    std::size_t free_offset = pending_offset + pending.size();

    BOOST_TEST(buff.reserved_first() != nullptr);
    BOOST_TEST(buff.current_message_first() == buff.reserved_first() + current_message_offset);
    BOOST_TEST(buff.pending_first() == buff.reserved_first() + pending_offset);
    BOOST_TEST(buff.free_first() == buff.reserved_first() + free_offset);

    BOOST_TEST(buff.reserved_area().data() == buff.reserved_first());
    BOOST_TEST(buff.current_message().data() == buff.reserved_first() + current_message_offset);
    BOOST_TEST(buff.pending_area().data() == buff.reserved_first() + pending_offset);
    BOOST_TEST(buff.free_area().data() == buff.reserved_first() + free_offset);

    BOOST_TEST(buff.reserved_size() == reserved.size()); 
    BOOST_TEST(buff.current_message_size() == current_message.size());
    BOOST_TEST(buff.pending_size() == pending.size());
    BOOST_TEST(buff.free_size() == buff.total_size() - free_offset);
    
    BOOST_TEST(buff.reserved_area().size() == reserved.size()); 
    BOOST_TEST(buff.current_message().size() == current_message.size());
    BOOST_TEST(buff.pending_area().size() == pending.size());
    BOOST_TEST(buff.free_area().size() == buff.total_size() - free_offset);
    
    BOOST_MYSQL_BUFF_TEST(buff.reserved_area(), buffer(reserved));
    BOOST_MYSQL_BUFF_TEST(buff.current_message(), buffer(current_message));
    BOOST_MYSQL_BUFF_TEST(buff.pending_area(), buffer(pending));
}


static void copy_to_free_area(read_buffer& buff, const std::vector<std::uint8_t>& bytes)
{
    std::copy(bytes.begin(), bytes.end(), buff.free_first());
}


// remove reserved without reserved, without other areas
// remove reserved without reserved, with other areas
// remove reserved with reserved, without other areas
// remove reserved with reserved, with other areas
// grow to fit with 0 request
// grow to fit but there is enough space
// grow to fit with other areas
// all fns on zero size buffer

BOOST_AUTO_TEST_SUITE(test_read_buffer)

BOOST_AUTO_TEST_SUITE(init_ctor)

BOOST_AUTO_TEST_CASE(some_initial_size)
{
    read_buffer buff (531);
    auto first = buff.reserved_first();
    
    BOOST_TEST(first != nullptr);
    BOOST_TEST(buff.free_size() == buff.total_size());
    BOOST_TEST(buff.total_size() >= 531);
    check_buffer(buff, {}, {}, {});
}

BOOST_AUTO_TEST_CASE(zero_initial_size)
{
    read_buffer buff (0);

    BOOST_TEST(buff.total_size() == 0);
    check_equal(buff.reserved_area(), const_buffer(), "free_area");
    check_equal(buff.current_message(), const_buffer(), "current_message");
    check_equal(buff.pending_area(), const_buffer(), "pending_area");
    check_equal(buff.free_area(), mutable_buffer(), "free_area");
}

BOOST_AUTO_TEST_SUITE_END()



BOOST_AUTO_TEST_SUITE(move_to_pending)

BOOST_AUTO_TEST_CASE(some_bytes)
{
    read_buffer buff (512);
    std::vector<std::uint8_t> contents { 0x01, 0x02, 0x03, 0x04 };
    copy_to_free_area(buff, contents);
    buff.move_to_pending(4);

    check_buffer(buff, {}, {}, contents);
}

BOOST_AUTO_TEST_CASE(all_bytes)
{
    read_buffer buff (8);
    std::vector<std::uint8_t> contents (buff.total_size(), 0x01);
    copy_to_free_area(buff, contents);
    buff.move_to_pending(buff.total_size());

    check_buffer(buff, {}, {}, contents);
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    read_buffer buff (8);
    buff.move_to_pending(0);

    check_buffer(buff, {}, {}, {});
}

BOOST_AUTO_TEST_CASE(several_calls)
{
    read_buffer buff (8);
    std::vector<std::uint8_t> contents { 0x01, 0x02, 0x03, 0x04 };
    copy_to_free_area(buff, contents);
    buff.move_to_pending(2);
    buff.move_to_pending(2);

    check_buffer(buff, {}, {}, contents);
}

BOOST_AUTO_TEST_SUITE_END()



BOOST_AUTO_TEST_SUITE(move_to_current_message)

BOOST_AUTO_TEST_CASE(some_bytes)
{
    read_buffer buff (8);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
    buff.move_to_pending(6);
    buff.move_to_current_message(2);

    check_buffer(buff, {}, { 0x01, 0x02 }, { 0x03, 0x04, 0x05, 0x06 });
}

BOOST_AUTO_TEST_CASE(all_bytes)
{
    read_buffer buff (8);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
    buff.move_to_pending(6);
    buff.move_to_current_message(6);

    check_buffer(buff, {}, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 }, {});
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    read_buffer buff (8);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
    buff.move_to_pending(6);
    buff.move_to_current_message(0);

    check_buffer(buff, {}, {}, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
}

BOOST_AUTO_TEST_CASE(several_calls)
{
    read_buffer buff (8);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
    buff.move_to_pending(6);
    buff.move_to_current_message(2);
    buff.move_to_current_message(3);

    check_buffer(buff, {}, { 0x01, 0x02, 0x03, 0x04, 0x05 }, { 0x06 });
}

BOOST_AUTO_TEST_SUITE_END()



BOOST_AUTO_TEST_SUITE(move_to_reserved)

BOOST_AUTO_TEST_CASE(some_bytes)
{
    read_buffer buff (8);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
    buff.move_to_pending(6);
    buff.move_to_current_message(5);
    buff.move_to_reserved(3);

    check_buffer(buff, { 0x01, 0x02, 0x03 }, { 0x04, 0x05 }, { 0x06 });
}

BOOST_AUTO_TEST_CASE(all_bytes)
{
    read_buffer buff (8);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
    buff.move_to_pending(6);
    buff.move_to_current_message(5);
    buff.move_to_reserved(5);

    check_buffer(buff, { 0x01, 0x02, 0x03, 0x04, 0x05 }, {}, { 0x06 });
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    read_buffer buff (8);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
    buff.move_to_pending(6);
    buff.move_to_current_message(5);
    buff.move_to_reserved(0);

    check_buffer(buff, {}, { 0x01, 0x02, 0x03, 0x04, 0x05 }, { 0x06 });
}

BOOST_AUTO_TEST_CASE(several_calls)
{
    read_buffer buff (8);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
    buff.move_to_pending(6);
    buff.move_to_current_message(5);
    buff.move_to_reserved(1);
    buff.move_to_reserved(2);

    check_buffer(buff, { 0x01, 0x02, 0x03 }, { 0x04, 0x05 }, { 0x06 });
}


BOOST_AUTO_TEST_SUITE_END()



BOOST_AUTO_TEST_SUITE(remove_current_message_last)

BOOST_AUTO_TEST_CASE(some_bytes)
{
    read_buffer buff (16);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 });
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    buff.move_to_reserved(1);
    buff.remove_current_message_last(2);

    check_buffer(buff, { 0x01 }, { 0x02, 0x03, 0x04 }, { 0x07, 0x08 });
}

BOOST_AUTO_TEST_CASE(all_bytes)
{
    read_buffer buff (16);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 });
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    buff.move_to_reserved(1);
    buff.remove_current_message_last(5);

    check_buffer(buff, { 0x01 }, {}, { 0x07, 0x08 });
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    read_buffer buff (16);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 });
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    buff.move_to_reserved(1);
    buff.remove_current_message_last(0);

    check_buffer(buff, { 0x01 }, { 0x02, 0x03, 0x04, 0x05, 0x06 }, { 0x07, 0x08 });
}

BOOST_AUTO_TEST_CASE(without_pending)
{
    read_buffer buff (16);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 });
    buff.move_to_pending(8);
    buff.move_to_current_message(8);
    buff.move_to_reserved(1);
    buff.remove_current_message_last(4);

    check_buffer(buff, { 0x01 }, { 0x02, 0x03, 0x04 }, {});
}

BOOST_AUTO_TEST_CASE(without_reserved)
{
    read_buffer buff (16);
    copy_to_free_area(buff, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 });
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    buff.remove_current_message_last(4);

    check_buffer(buff, {}, { 0x01, 0x02 }, { 0x07, 0x08 });
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
