//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/sansio/vector_write_adaptor.hpp>

#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"

using namespace boost::mysql::detail;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_vector_write_adaptor)

BOOST_AUTO_TEST_CASE(initial_state)
{
    std::vector<std::uint8_t> vec;
    vector_write_adaptor adaptor(vec);

    BOOST_TEST(adaptor.size() == 0u);
    BOOST_TEST(adaptor.data() == vec.data());

    // implicit span conversion should be empty
    boost::span<const std::uint8_t> s = adaptor;
    BOOST_TEST(s.size() == 0u);
}

BOOST_AUTO_TEST_CASE(append)
{
    std::vector<std::uint8_t> vec;
    vector_write_adaptor adaptor(vec);

    const std::vector<std::uint8_t> part1{0x01, 0x02};
    const std::vector<std::uint8_t> part2{0x03, 0x04};
    const std::vector<std::uint8_t> part3{0x05, 0x06, 0x07};

    adaptor.append(part1);
    BOOST_TEST(adaptor.size() == 2u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, std::vector<std::uint8_t>({0x01, 0x02}));
    adaptor.append(part2);
    BOOST_TEST(adaptor.size() == 4u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, std::vector<std::uint8_t>({0x01, 0x02, 0x03, 0x04}));
    adaptor.append(part3);
    BOOST_TEST(adaptor.size() == 7u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, std::vector<std::uint8_t>({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}));
}

BOOST_AUTO_TEST_CASE(append_grows_vector)
{
    std::vector<std::uint8_t> vec;
    vector_write_adaptor adaptor(vec);

    const std::vector<std::uint8_t> data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    adaptor.append(data);

    BOOST_TEST(adaptor.size() == 8u);
    BOOST_TEST(vec.size() == 8u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, data);
}

BOOST_AUTO_TEST_CASE(preserves_existing_content)
{
    // Unlike rw_buffer, there is no reserved/pending split: the adaptor appends
    // after whatever the vector already holds, and exposes the whole vector.
    std::vector<std::uint8_t> vec{0x04, 0x05, 0x06};
    vector_write_adaptor adaptor(vec);

    BOOST_TEST(adaptor.data() == vec.data());
    BOOST_TEST(adaptor.size() == 3u);

    const std::vector<std::uint8_t> data{0xAA, 0xBB, 0xCC};
    adaptor.append(data);

    // Existing content intact, new bytes went to the end.
    const std::vector<std::uint8_t> expected{0x04, 0x05, 0x06, 0xAA, 0xBB, 0xCC};
    BOOST_TEST(vec.size() == 6u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(vec, expected);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, expected);
}

BOOST_AUTO_TEST_CASE(append_empty)
{
    std::vector<std::uint8_t> vec{0x01, 0x02};
    vector_write_adaptor adaptor(vec);

    BOOST_TEST(adaptor.append(boost::span<const std::uint8_t>()) == error_code());
    BOOST_TEST(adaptor.size() == 2u);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, std::vector<std::uint8_t>({0x01, 0x02}));

    // Empty append on an empty vector is a no-op too (insert with empty range).
    std::vector<std::uint8_t> empty_vec;
    vector_write_adaptor empty_adaptor(empty_vec);
    BOOST_TEST(empty_adaptor.append(boost::span<const std::uint8_t>()) == error_code());
    BOOST_TEST(empty_adaptor.size() == 0u);
}

BOOST_AUTO_TEST_CASE(append_never_fails)
{
    // No max-buffer-size limit here (contrast with write_adaptor/rw_buffer).
    std::vector<std::uint8_t> vec;
    vector_write_adaptor adaptor(vec);

    const std::vector<std::uint8_t> data(1024 * 1024, 0xAB);
    BOOST_TEST(adaptor.append(data) == error_code());
    BOOST_TEST(adaptor.size() == data.size());
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(adaptor, data);
}

BOOST_AUTO_TEST_CASE(sequential_appends)
{
    std::vector<std::uint8_t> vec;
    const std::vector<std::uint8_t> stage1{0x01, 0x02};
    const std::vector<std::uint8_t> stage2{0x03, 0x04, 0x05};

    {
        vector_write_adaptor adaptor(vec);
        adaptor.append(stage1);
    }
    {
        vector_write_adaptor adaptor(vec);
        adaptor.append(stage2);
    }

    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(vec, std::vector<std::uint8_t>({0x01, 0x02, 0x03, 0x04, 0x05}));
}

BOOST_AUTO_TEST_SUITE_END()
