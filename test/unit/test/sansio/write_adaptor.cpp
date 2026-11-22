//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/impl/internal/sansio/rw_buffer.hpp>
#include <boost/mysql/impl/internal/sansio/write_adaptor.hpp>

#include <boost/test/unit_test.hpp>

#include <cstring>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"

using namespace boost::mysql::detail;

BOOST_AUTO_TEST_SUITE(test_write_adaptor)

BOOST_AUTO_TEST_CASE(initial_state)
{
    rw_buffer buff(16);
    write_adaptor adaptor(buff);

    BOOST_TEST(adaptor.size() == 0u);
    BOOST_TEST(adaptor.data() == buff.free_first());
    BOOST_TEST(adaptor.end() == buff.free_first());
    BOOST_TEST(adaptor.data() == adaptor.end());

    // implicit span conversion should be empty
    boost::span<const std::uint8_t> s = adaptor;
    BOOST_TEST(s.size() == 0u);
}

BOOST_AUTO_TEST_CASE(clear)
{
    rw_buffer buff(16);
    write_adaptor adaptor(buff);

    const std::vector<std::uint8_t> data{0x01, 0x02, 0x03};
    adaptor.insert(adaptor.data(), data.data(), data.data() + data.size());

    BOOST_TEST(adaptor.size() == 3u);

    adaptor.clear();
    BOOST_TEST(adaptor.size() == 0u);
    BOOST_TEST(adaptor.end() == adaptor.data());
    BOOST_TEST(adaptor.end() == buff.free_first());
}

BOOST_AUTO_TEST_CASE(insert_empty)
{
    rw_buffer buff(16);
    write_adaptor adaptor(buff);

    const std::vector<std::uint8_t> data{0x01, 0x02, 0x03};
    adaptor.insert(adaptor.data(), data.data(), data.data() + data.size());

    BOOST_TEST(adaptor.size() == 3u);
    BOOST_TEST(adaptor.end() == adaptor.data() + 3);

    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, data);
}

BOOST_AUTO_TEST_CASE(insert_append)
{
    rw_buffer buff(16);
    write_adaptor adaptor(buff);

    const std::vector<std::uint8_t> part1{0x01, 0x02};
    const std::vector<std::uint8_t> part2{0x03, 0x04};

    adaptor.insert(adaptor.data(), part1.data(), part1.data() + part1.size());
    adaptor.insert(adaptor.end(), part2.data(), part2.data() + part2.size());

    BOOST_TEST(adaptor.size() == 4u);

    const std::vector<std::uint8_t> expected{0x01, 0x02, 0x03, 0x04};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, expected);
}

BOOST_AUTO_TEST_CASE(insert_beggining)
{
    rw_buffer buff(16);
    write_adaptor adaptor(buff);

    const std::vector<std::uint8_t> initial{0x01, 0x02, 0x03, 0x04, 0x05};
    adaptor.insert(adaptor.data(), initial.data(), initial.data() + initial.size());

    const std::vector<std::uint8_t> overwrite{0xAA, 0xBB};
    adaptor.insert(adaptor.data(), overwrite.data(), overwrite.data() + overwrite.size());

    BOOST_TEST(adaptor.size() == 7u);
    std::vector<std::uint8_t> expected{0xAA, 0xBB, 0x01, 0x02, 0x03, 0x04, 0x05};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, expected);
}

BOOST_AUTO_TEST_CASE(insert_middle)
{
    rw_buffer buff(16);
    write_adaptor adaptor(buff);

    const std::vector<std::uint8_t> initial{0x01, 0x02, 0x03, 0x04, 0x05};
    adaptor.insert(adaptor.data(), initial.data(), initial.data() + initial.size());

    const std::vector<std::uint8_t> overwrite{0xAA, 0xBB};
    adaptor.insert(adaptor.data() + 1, overwrite.data(), overwrite.data() + overwrite.size());

    BOOST_TEST(adaptor.size() == 7u);
    std::vector<std::uint8_t> expected{0x01, 0xAA, 0xBB, 0x02, 0x03, 0x04, 0x05};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, expected);
}

BOOST_AUTO_TEST_CASE(insert_triggers_growth)
{
    rw_buffer buff(4);
    write_adaptor adaptor(buff);

    const std::vector<std::uint8_t> data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    adaptor.insert(adaptor.data(), data.data(), data.data() + data.size());

    BOOST_TEST(adaptor.size() == 8u);
    BOOST_TEST(buff.size() == 8u);

    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, data);
}

BOOST_AUTO_TEST_CASE(preserves_pending)
{
    rw_buffer buff(16);

    const std::vector<std::uint8_t> reserved{0x01};
    const std::vector<std::uint8_t> current{0x02, 0x03};
    const std::vector<std::uint8_t> pending{0x04, 0x05, 0x06};

    std::uint8_t* p = buff.free_first();
    std::memcpy(p, reserved.data(), reserved.size());
    p += reserved.size();
    std::memcpy(p, current.data(), current.size());
    p += current.size();
    std::memcpy(p, pending.data(), pending.size());

    buff.move_to_pending(reserved.size() + current.size() + pending.size());
    buff.move_to_current_message(reserved.size() + current.size());
    buff.move_to_reserved(reserved.size());

    BOOST_TEST(buff.reserved_size() == 1u);
    BOOST_TEST(buff.current_message_size() == 2u);
    BOOST_TEST(buff.pending_size() == 3u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff.pending_area(), pending);

    // Reclaim space for writing the way connection_state_data::write() does:
    // "reclassify" the current message as reserved, then drop everything reserved.
    buff.move_to_reserved(buff.current_message_size());
    buff.remove_reserved();

    // Pending survived the compaction (it's now at the front of the buffer).
    BOOST_TEST(buff.reserved_size() == 0u);
    BOOST_TEST(buff.current_message_size() == 0u);
    BOOST_TEST(buff.pending_size() == 3u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff.pending_area(), pending);

    // Write via adaptor: the write must land after pending, not over it.
    write_adaptor adaptor(buff);
    BOOST_TEST(adaptor.data() == buff.pending_first() + buff.pending_size());

    const std::vector<std::uint8_t> data{0xAA, 0xBB, 0xCC};
    adaptor.insert(adaptor.data(), data.data(), data.data() + data.size());

    // Pending still intact after the write.
    BOOST_TEST(buff.pending_size() == 3u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff.pending_area(), pending);

    // Written bytes went after pending.
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, data);

    // Full layout is [pending][written].
    const std::vector<std::uint8_t> expected{0x04, 0x05, 0x06, 0xAA, 0xBB, 0xCC};
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
        boost::span<const std::uint8_t>(buff.pending_first(), buff.pending_size() + adaptor.size()),
        expected
    );
}

BOOST_AUTO_TEST_SUITE_END()
