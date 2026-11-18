//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/sansio/rw_buffer.hpp>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/printing.hpp"

using namespace boost::mysql::detail;
using boost::mysql::client_errc;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_rw_buffer)

// Records the buffer first pointer and size to verify the buffer
// didn't do any re-allocation
class stability_checker
{
    rw_buffer& b_;
    const std::uint8_t* first_;
    std::size_t total_size_;

public:
    stability_checker(rw_buffer& b) noexcept : b_(b), first_(b.first()), total_size_(b.capacity()) {}
    void check_stability()
    {
        BOOST_TEST(b_.first() == first_);
        BOOST_TEST(b_.capacity() == total_size_);
    }
    void check_reallocation()
    {
        BOOST_TEST(b_.first() != first_);
        BOOST_TEST(b_.capacity() > total_size_);
    }
};

static void check_buffer(
    rw_buffer& buff,
    const std::uint8_t* reserved_first,
    const std::uint8_t* current_message_first,
    const std::uint8_t* pending_first,
    const std::uint8_t* free_first,
    std::size_t reserved_size,
    std::size_t current_message_size,
    std::size_t pending_size,
    std::size_t free_size
)
{
    BOOST_TEST(buff.reserved_first() == reserved_first);
    BOOST_TEST(buff.current_message_first() == current_message_first);
    BOOST_TEST(buff.pending_first() == pending_first);
    BOOST_TEST(buff.free_first() == free_first);

    BOOST_TEST(buff.reserved_area().data() == reserved_first);
    BOOST_TEST(buff.current_message().data() == current_message_first);
    BOOST_TEST(buff.pending_area().data() == pending_first);
    BOOST_TEST(buff.free_area().data() == free_first);

    BOOST_TEST(buff.reserved_size() == reserved_size);
    BOOST_TEST(buff.current_message_size() == current_message_size);
    BOOST_TEST(buff.pending_size() == pending_size);
    BOOST_TEST(buff.free_size() == free_size);

    BOOST_TEST(buff.reserved_area().size() == reserved_size);
    BOOST_TEST(buff.current_message().size() == current_message_size);
    BOOST_TEST(buff.pending_area().size() == pending_size);
    BOOST_TEST(buff.free_area().size() == free_size);

    BOOST_TEST(buff.capacity() == reserved_size + current_message_size + pending_size + free_size);
}

static void check_buffer(
    rw_buffer& buff,
    const std::vector<std::uint8_t>& reserved,
    const std::vector<std::uint8_t>& current_message,
    const std::vector<std::uint8_t>& pending,
    std::size_t free_size
)
{
    std::size_t current_message_offset = reserved.size();
    std::size_t pending_offset = current_message_offset + current_message.size();
    std::size_t free_offset = pending_offset + pending.size();

    BOOST_TEST(buff.first() != nullptr);

    check_buffer(
        buff,
        buff.first(),
        buff.first() + current_message_offset,
        buff.first() + pending_offset,
        buff.first() + free_offset,
        reserved.size(),
        current_message.size(),
        pending.size(),
        free_size
    );

    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff.reserved_area(), reserved);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff.current_message(), current_message);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buff.pending_area(), pending);
}

static void check_empty_buffer(rw_buffer& buff)
{
    check_buffer(buff, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0);
}

static void copy_to_free_area(rw_buffer& buff, const std::vector<std::uint8_t>& bytes)
{
    std::copy(bytes.begin(), bytes.end(), buff.free_first());
}

BOOST_AUTO_TEST_SUITE(init_ctor)

BOOST_AUTO_TEST_CASE(some_initial_size)
{
    rw_buffer buff(531);
    check_buffer(buff, {}, {}, {}, 531);
}

BOOST_AUTO_TEST_CASE(zero_initial_size)
{
    rw_buffer buff(0);

    check_empty_buffer(buff);

    // Calling all other functions with 0 values on this buffer doesn't cause UB
    buff.move_to_pending(0);
    buff.move_to_current_message(0);
    buff.move_to_reserved(0);
    buff.remove_reserved();
    auto ec = buff.grow_to_fit(0);
    BOOST_TEST(ec == error_code());
    check_empty_buffer(buff);
}

BOOST_AUTO_TEST_CASE(initial_size_eq_max_size)
{
    rw_buffer buff(16, 16);
    check_buffer(buff, {}, {}, {}, 16);
    BOOST_TEST(buff.max_size() == 16u);

    // Using the buffer works normally
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04});
    buff.move_to_pending(4);
    buff.move_to_current_message(3);
    buff.move_to_reserved(1);
    check_buffer(buff, {0x01}, {0x02, 0x03}, {0x04}, 12);

    // Growing works
    auto ec = buff.grow_to_fit(12);
    BOOST_TEST(ec == error_code());
    ec = buff.grow_to_fit(13);
    BOOST_TEST(ec == client_errc::max_buffer_size_exceeded);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(move_to_pending)

BOOST_AUTO_TEST_CASE(some_bytes)
{
    rw_buffer buff(512);
    stability_checker checker(buff);
    std::vector<std::uint8_t> contents{0x01, 0x02, 0x03, 0x04};
    copy_to_free_area(buff, contents);
    buff.move_to_pending(4);

    check_buffer(buff, {}, {}, contents, 508);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(all_bytes)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    std::vector<std::uint8_t> contents(8, 0x01);
    copy_to_free_area(buff, contents);
    buff.move_to_pending(buff.capacity());

    check_buffer(buff, {}, {}, contents, 0);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    buff.move_to_pending(0);

    check_buffer(buff, {}, {}, {}, 8);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(several_calls)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    std::vector<std::uint8_t> contents{0x01, 0x02, 0x03, 0x04};
    copy_to_free_area(buff, contents);
    buff.move_to_pending(2);
    buff.move_to_pending(2);

    check_buffer(buff, {}, {}, contents, 4);
    checker.check_stability();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(move_to_current_message)

BOOST_AUTO_TEST_CASE(some_bytes)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    buff.move_to_pending(6);
    buff.move_to_current_message(2);

    check_buffer(buff, {}, {0x01, 0x02}, {0x03, 0x04, 0x05, 0x06}, 2);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(all_bytes)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    buff.move_to_pending(6);
    buff.move_to_current_message(6);

    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {}, 2);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    buff.move_to_pending(6);
    buff.move_to_current_message(0);

    check_buffer(buff, {}, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, 2);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(several_calls)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    buff.move_to_pending(6);
    buff.move_to_current_message(2);
    buff.move_to_current_message(3);

    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05}, {0x06}, 2);
    checker.check_stability();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(move_to_reserved)

BOOST_AUTO_TEST_CASE(some_bytes)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    buff.move_to_pending(6);
    buff.move_to_current_message(5);
    buff.move_to_reserved(3);

    check_buffer(buff, {0x01, 0x02, 0x03}, {0x04, 0x05}, {0x06}, 2);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(all_bytes)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    buff.move_to_pending(6);
    buff.move_to_current_message(5);
    buff.move_to_reserved(5);

    check_buffer(buff, {0x01, 0x02, 0x03, 0x04, 0x05}, {}, {0x06}, 2);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    buff.move_to_pending(6);
    buff.move_to_current_message(5);
    buff.move_to_reserved(0);

    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05}, {0x06}, 2);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(several_calls)
{
    rw_buffer buff(8);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    buff.move_to_pending(6);
    buff.move_to_current_message(5);
    buff.move_to_reserved(1);
    buff.move_to_reserved(2);

    check_buffer(buff, {0x01, 0x02, 0x03}, {0x04, 0x05}, {0x06}, 2);
    checker.check_stability();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(remove_current_message_last)

BOOST_AUTO_TEST_CASE(some_bytes)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    buff.move_to_reserved(1);
    buff.remove_current_message_last(2);

    check_buffer(buff, {0x01}, {0x02, 0x03, 0x04}, {0x07, 0x08}, 10);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(all_bytes)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    buff.move_to_reserved(1);
    buff.remove_current_message_last(5);

    check_buffer(buff, {0x01}, {}, {0x07, 0x08}, 13);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(without_pending)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(8);
    buff.move_to_reserved(1);
    buff.remove_current_message_last(4);

    check_buffer(buff, {0x01}, {0x02, 0x03, 0x04}, {}, 12);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(without_reserved)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    buff.remove_current_message_last(4);

    check_buffer(buff, {}, {0x01, 0x02}, {0x07, 0x08}, 12);
    checker.check_stability();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(remove_reserved)

BOOST_AUTO_TEST_CASE(with_other_areas)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    buff.move_to_reserved(2);
    buff.remove_reserved();

    check_buffer(buff, {}, {0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 10);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(without_other_areas)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(8);
    buff.move_to_reserved(8);
    buff.remove_reserved();

    check_buffer(buff, {}, {}, {}, 16);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    buff.remove_reserved();

    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 8);
    checker.check_stability();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(grow_to_fit)

BOOST_AUTO_TEST_CASE(not_enough_space)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    auto ec = buff.grow_to_fit(100);

    BOOST_TEST(ec == error_code());
    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 120);
    checker.check_reallocation();
}

BOOST_AUTO_TEST_CASE(one_missing_byte)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);

    auto ec = buff.grow_to_fit(9);

    BOOST_TEST(ec == error_code());
    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 24);
    checker.check_reallocation();
}

BOOST_AUTO_TEST_CASE(enough_space)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    auto ec = buff.grow_to_fit(8);
    BOOST_TEST(ec == error_code());

    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 8);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(zero_bytes)
{
    rw_buffer buff(16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);
    auto ec = buff.grow_to_fit(0);
    BOOST_TEST(ec == error_code());

    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 8);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(from_size_0)
{
    // Regression check: growing from size 0 works
    rw_buffer buff(0, 1024);
    check_empty_buffer(buff);

    auto ec = buff.grow_to_fit(16);
    BOOST_TEST(ec == error_code());
    check_buffer(buff, {}, {}, {}, 16);
}

BOOST_AUTO_TEST_CASE(lt_max_size)
{
    rw_buffer buff(8, 16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);

    // Grow past the current size, but not reaching max size
    auto ec = buff.grow_to_fit(7);
    BOOST_TEST(ec == error_code());

    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 8);
    checker.check_reallocation();
}

BOOST_AUTO_TEST_CASE(eq_max_size)
{
    rw_buffer buff(8, 16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);

    // Grow past the current size, reaching max_size
    auto ec = buff.grow_to_fit(8);
    BOOST_TEST(ec == error_code());

    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 8);
    checker.check_reallocation();
}

BOOST_AUTO_TEST_CASE(gt_max_size)
{
    rw_buffer buff(8, 16);
    stability_checker checker(buff);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);

    // Try to grow past max size
    auto ec = buff.grow_to_fit(10);
    BOOST_TEST(ec == client_errc::max_buffer_size_exceeded);
    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 0);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(several_grows)
{
    rw_buffer buff(8, 16);
    copy_to_free_area(buff, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    buff.move_to_pending(8);
    buff.move_to_current_message(6);

    // Grow with reallocation
    auto ec = buff.grow_to_fit(4);
    BOOST_TEST(ec == error_code());
    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08}, 8);

    // Place some more bytes in the buffer
    copy_to_free_area(buff, {0x09, 0x0a});
    buff.move_to_pending(2);
    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08, 0x09, 0x0a}, 6);

    // Grow without reallocation
    ec = buff.grow_to_fit(2);
    BOOST_TEST(ec == error_code());
    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08, 0x09, 0x0a}, 6);
    copy_to_free_area(buff, {0x0b, 0x0c});
    buff.move_to_pending(2);
    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c}, 4);

    // Fail when attempting to grow past max size
    ec = buff.grow_to_fit(5);
    BOOST_TEST(ec == client_errc::max_buffer_size_exceeded);
    check_buffer(buff, {}, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, {0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c}, 4);
}

BOOST_AUTO_TEST_CASE(is_power_of_two)
{
    rw_buffer buff(8);

    std::size_t test_sizes[] = {
        5, 7, 8,
        9, 15, 16,
        17, 31, 32,
        33, 63, 64,
        65, 127, 128,
        129, 255, 256,
        257, 511, 512, 513
    };

    // Test that buffer capacity grows to powers of two
    for (auto new_size : test_sizes)
    {
        BOOST_TEST_CONTEXT(new_size)
        {
            std::size_t next_power_of_two = 1;
            while (next_power_of_two < new_size)
                next_power_of_two *= 2;
            auto ec = buff.grow_to_fit(new_size);
            BOOST_TEST(ec == error_code());
            BOOST_TEST(buff.capacity() == next_power_of_two);
        }
    }
    BOOST_TEST(buff.capacity() == 1024u);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(reset)

BOOST_AUTO_TEST_CASE(zero_size_buffer)
{
    rw_buffer buff(0, 1024);
    buff.reset();
    BOOST_TEST(buff.capacity() == 0u);
    BOOST_TEST(buff.max_size() == 1024u);
}

BOOST_AUTO_TEST_CASE(free_buffer)
{
    rw_buffer buff(16, 1024);
    stability_checker checker(buff);
    buff.reset();
    check_buffer(buff, {}, {}, {}, 16);
    checker.check_stability();
    BOOST_TEST(buff.max_size() == 1024u);
}

BOOST_AUTO_TEST_CASE(pending_bytes)
{
    rw_buffer buff(16, 512);
    stability_checker checker(buff);
    buff.move_to_pending(4);
    buff.reset();
    check_buffer(buff, {}, {}, {}, 16);
    checker.check_stability();
    BOOST_TEST(buff.max_size() == 512u);
}

BOOST_AUTO_TEST_CASE(current_message_bytes)
{
    rw_buffer buff(16, 512);
    stability_checker checker(buff);
    buff.move_to_pending(4);
    buff.move_to_current_message(4);
    buff.reset();
    check_buffer(buff, {}, {}, {}, 16);
    checker.check_stability();
    BOOST_TEST(buff.max_size() == 512u);
}

BOOST_AUTO_TEST_CASE(reserved_bytes)
{
    rw_buffer buff(16, 512u);
    stability_checker checker(buff);
    buff.move_to_pending(4);
    buff.move_to_current_message(4);
    buff.move_to_reserved(4);
    buff.reset();
    check_buffer(buff, {}, {}, {}, 16);
    checker.check_stability();
    BOOST_TEST(buff.max_size() == 512u);
}

BOOST_AUTO_TEST_CASE(bytes_in_all_areas)
{
    rw_buffer buff(16, 1024);
    stability_checker checker(buff);
    buff.move_to_pending(10);
    buff.move_to_current_message(8);
    buff.move_to_reserved(2);
    buff.reset();
    check_buffer(buff, {}, {}, {}, 16);
    checker.check_stability();
    BOOST_TEST(buff.max_size() == 1024u);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(write_area)

// Fresh buffer: write_offset_ == 0, data()/end() point to first(), span empty
BOOST_AUTO_TEST_CASE(initial_state)
{
    rw_buffer buff(16);
    BOOST_TEST(buff.size() == 0u);
    BOOST_TEST(buff.data() == buff.first());
    BOOST_TEST(buff.end() == buff.first());
    BOOST_TEST(buff.data() == buff.end());

    boost::span<const std::uint8_t> s = buff;
    BOOST_TEST(s.size() == 0u);
    BOOST_TEST(s.data() == buff.first());
}

// clear() resets the write position, but not the underlying buffer,
// and does not touch the read areas
BOOST_AUTO_TEST_CASE(clear_resets_write_area)
{
    rw_buffer buff(16);
    stability_checker checker(buff);

    buff.push_back(0x01);
    buff.push_back(0x02);
    BOOST_TEST(buff.size() == 2u);

    buff.clear();
    BOOST_TEST(buff.size() == 0u);
    BOOST_TEST(buff.end() == buff.data());

    // Read areas are untouched
    check_buffer(buff, {}, {}, {}, 16);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(push_back_within_capacity)
{
    rw_buffer buff(16);
    stability_checker checker(buff);

    buff.push_back(0x01);
    buff.push_back(0x02);
    buff.push_back(0x03);

    BOOST_TEST(buff.size() == 3u);
    BOOST_TEST(buff.end() == buff.first() + 3);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
        boost::span<const std::uint8_t>(buff),
        (std::vector<std::uint8_t>{0x01, 0x02, 0x03})
    );
    // Read areas are still all at 0
    check_buffer(buff, {}, {}, {}, 16);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(push_back_from_empty_buffer)
{
    rw_buffer buff(0, 1024);
    buff.push_back(0x01);

    BOOST_TEST(buff.size() == 1u);
    BOOST_TEST(buff.capacity() == 1u);
}

BOOST_AUTO_TEST_CASE(push_back_triggers_growth)
{
    rw_buffer buff(4);
    stability_checker checker(buff);

    buff.push_back(0x01);
    buff.push_back(0x02);
    buff.push_back(0x03);
    buff.push_back(0x04);
    BOOST_TEST(buff.size() == 4u);
    checker.check_stability();

    buff.push_back(0x05);
    BOOST_TEST(buff.size() == 5u);
    BOOST_TEST(buff.capacity() == 8u);
    checker.check_reallocation();

    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
        boost::span<const std::uint8_t>(buff),
        (std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05})
    );
}

BOOST_AUTO_TEST_CASE(resize_within_capacity)
{
    rw_buffer buff(16);
    stability_checker checker(buff);

    buff.resize(4);
    BOOST_TEST(buff.size() == 4u);

    buff.resize(8);
    BOOST_TEST(buff.size() == 8u);

    // Shrinking only affects write_offset_, not the underlying buffer
    buff.resize(2);
    BOOST_TEST(buff.size() == 2u);
    BOOST_TEST(buff.capacity() == 16u);

    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(resize_grows_to_power_of_two)
{
    rw_buffer buff(4);
    stability_checker checker(buff);

    buff.resize(5);
    BOOST_TEST(buff.size() == 5u);
    BOOST_TEST(buff.capacity() == 8u);
    checker.check_reallocation();

    // Already a power of two: no extra growth
    buff.resize(16);
    BOOST_TEST(buff.size() == 16u);
    BOOST_TEST(buff.capacity() == 16u);
}

BOOST_AUTO_TEST_CASE(resize_respects_max_size)
{
    // Requesting up to max_size is fine
    rw_buffer buff(4, 12);
    stability_checker checker(buff);

    buff.resize(12);
    BOOST_TEST(buff.size() == 12u);
    BOOST_TEST(buff.capacity() == 12u);
    checker.check_reallocation();
}

BOOST_AUTO_TEST_CASE(insert_into_empty_buffer)
{
    rw_buffer buff(16);
    stability_checker checker(buff);

    const std::vector<std::uint8_t> bytes{0x01, 0x02, 0x03};
    buff.insert(buff.data(), bytes.data(), bytes.data() + bytes.size());

    BOOST_TEST(buff.size() == 3u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(boost::span<const std::uint8_t>(buff), bytes);
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(insert_appends_at_end)
{
    rw_buffer buff(16);
    stability_checker checker(buff);

    buff.push_back(0x01);
    buff.push_back(0x02);
    buff.push_back(0x03);

    const std::vector<std::uint8_t> tail{0x04, 0x05};
    buff.insert(buff.end(), tail.data(), tail.data() + tail.size());

    BOOST_TEST(buff.size() == 5u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
        boost::span<const std::uint8_t>(buff),
        (std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05})
    );
    checker.check_stability();
}

BOOST_AUTO_TEST_CASE(insert_triggers_growth)
{
    rw_buffer buff(4);
    stability_checker checker(buff);

    buff.push_back(0x01);
    buff.push_back(0x02);
    buff.push_back(0x03);
    buff.push_back(0x04);
    checker.check_stability();

    // Inserting at end() forces realloc — insert() computes the offset before
    // resize(), so the (pre-realloc) end() pointer stays safe
    const std::vector<std::uint8_t> tail{0x05, 0x06, 0x07};
    buff.insert(buff.end(), tail.data(), tail.data() + tail.size());

    BOOST_TEST(buff.size() == 7u);
    BOOST_TEST(buff.capacity() == 8u);
    checker.check_reallocation();

    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
        boost::span<const std::uint8_t>(buff),
        (std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07})
    );
}

BOOST_AUTO_TEST_CASE(implicit_span_conversion)
{
    rw_buffer buff(8);
    buff.push_back(0x01);
    buff.push_back(0x02);
    buff.push_back(0x03);

    // Implicit conversion used by next_action::write({write_buffer, ...})
    boost::span<const std::uint8_t> s = buff;
    BOOST_TEST(s.size() == 3u);
    BOOST_TEST(s.data() == buff.data());
    BOOST_TEST(s[0] == 0x01);
    BOOST_TEST(s[1] == 0x02);
    BOOST_TEST(s[2] == 0x03);
}

// The realistic lifecycle: consume a read message via reset(), then write
BOOST_AUTO_TEST_CASE(write_after_reset)
{
    rw_buffer buff(16);
    stability_checker checker(buff);

    // Simulate a completed read cycle
    copy_to_free_area(buff, {0x10, 0x11, 0x12, 0x13});
    buff.move_to_pending(4);
    buff.move_to_current_message(4);
    buff.move_to_reserved(4);
    BOOST_TEST(buff.pending_size() == 0u);

    buff.reset();
    check_buffer(buff, {}, {}, {}, 16);

    buff.push_back(0xA0);
    buff.push_back(0xA1);
    BOOST_TEST(buff.size() == 2u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(
        boost::span<const std::uint8_t>(buff),
        (std::vector<std::uint8_t>{0xA0, 0xA1})
    );
    // Read areas are still all zero after reset
    check_buffer(buff, {}, {}, {}, 16);
    checker.check_stability();
}

// After a write cycle, clear() + read cycle works normally
BOOST_AUTO_TEST_CASE(read_after_write)
{
    rw_buffer buff(16);
    stability_checker checker(buff);

    buff.push_back(0xAA);
    buff.push_back(0xBB);
    BOOST_TEST(buff.size() == 2u);

    buff.clear();
    BOOST_TEST(buff.size() == 0u);

    // Read areas are unaffected by write + clear; use them as usual
    copy_to_free_area(buff, {0x01, 0x02, 0x03});
    buff.move_to_pending(3);
    buff.move_to_current_message(2);

    check_buffer(buff, {}, {0x01, 0x02}, {0x03}, 13);
    checker.check_stability();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
