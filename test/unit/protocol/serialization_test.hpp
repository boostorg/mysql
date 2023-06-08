//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_PROTOCOL_SERIALIZATION_TEST_HPP
#define BOOST_MYSQL_TEST_UNIT_PROTOCOL_SERIALIZATION_TEST_HPP

#include <boost/asio/buffer.hpp>
#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include "protocol/serialization.hpp"
#include "test_common/assert_buffer_equals.hpp"

/**
 * TODO: this is all simpler now
 * This header defines the required infrastructure for running
 * **serialization tests**. These are based on inspecting real packets
 * with a network analyzer, like Wireshark, to create a "golden file",
 * and verify that our serialization functions generate the same
 * packets as the official MySQL client and server.
 *
 * Each serialization test is defined by an instance of a
 * serialization_sample, which contains a C++ value
 * and its serialized network representation, as a byte array.
 * The interface any_value uses type erasure to represent any C++ value
 * in a generic way, providing access to the required serialization
 * and reporting functions.
 *
 * For each type that can be serialized or deserialized, we define
 * a const serialization_test_spec variable in one of the header
 * files in serialization_test_samples/. This contains a vector of
 * serialization_sample and a serialization_test_type, which defines
 * which of the three following types of test to run:
 *   - serialize: checks serialize() and get_size()
 *   - deserialize: checks deserialize()
 *   - deserialize_space: checks deserialize() under extra bytes and
 *     not enough space conditions. Some messages can't pass these tests,
 *     as their contents depends on message size (e.g. string_eof).
 *
 * Types may run one or more of the above types.
 *
 * All header files in serialization_test_samples/ are included
 * in serialization_test.cpp and the defined variables are used to
 * run the adequate tests.
 */

namespace boost {
namespace mysql {
namespace test {

template <class T>
void do_serialize_test(T value, span<const std::uint8_t> serialized)
{
    // Size
    std::size_t expected_size = serialized.size();
    std::size_t actual_size = detail::get_size(value);
    BOOST_TEST(actual_size == expected_size);

    // Serialize
    std::vector<uint8_t> buffer(expected_size + 8, 0x7a);  // buffer overrun detector
    detail::serialization_context ctx(buffer.data());
    detail::serialize(ctx, value);

    // Check buffer
    asio::const_buffer expected_populated(serialized.data(), serialized.size());
    asio::const_buffer actual_populated(buffer.data(), actual_size);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(expected_populated, actual_populated);

    // Iterator
    BOOST_TEST(ctx.first() == buffer.data() + expected_size, "Iterator not updated correctly");

    // Check for buffer overruns
    std::vector<std::uint8_t> expected_clean(8, 0x7a);
    asio::const_buffer actual_clean(buffer.data() + expected_size, 8);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(asio::buffer(expected_clean), actual_clean);
}

template <class T>
void do_deserialize_test(T value, span<const std::uint8_t> serialized)
{
    auto first = serialized.data();
    auto size = serialized.size();
    detail::deserialization_context ctx(first, first + size);
    T actual{};
    detail::deserialize_errc err = detail::deserialize(ctx, actual);

    // No error
    BOOST_TEST(err == detail::deserialize_errc::ok);

    // Iterator advanced
    BOOST_TEST(ctx.first() == first + size);

    // Actual value
    BOOST_TEST(actual == value);
}

template <class T>
void do_deserialize_extra_space_test(T value, span<const std::uint8_t> serialized)
{
    // Create a buffer with extra data
    std::vector<std::uint8_t> buffer(serialized.begin(), serialized.end());
    buffer.push_back(0xff);

    // Deserialize
    detail::deserialization_context ctx(buffer.data(), buffer.size());
    T actual{};
    detail::deserialize_errc err = detail::deserialize(ctx, actual);

    // No error
    BOOST_TEST(err == detail::deserialize_errc::ok);

    // Iterator advanced
    BOOST_TEST(ctx.first() == buffer.data() + serialized.size());

    // Actual value
    BOOST_TEST(actual == value);
}

template <class T>
void do_deserialize_not_enough_space_test(span<const std::uint8_t> serialized)
{
    std::vector<uint8_t> buffer(serialized.begin(), serialized.end());
    buffer.back() = 0x7a;  // try to detect any overruns
    detail::deserialization_context ctx(buffer.data(), buffer.data() + buffer.size() - 1);

    T value{};
    detail::deserialize_errc err = boost::mysql::detail::deserialize(ctx, value);
    BOOST_TEST(err == detail::deserialize_errc::incomplete_message);
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
