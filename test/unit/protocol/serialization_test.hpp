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

#include <algorithm>
#include <cstring>
#include <initializer_list>

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

// A special buffer for serialization tests. Installs an overrun detector at the end to facilitate overrun
// detection
class serialization_buffer
{
    std::size_t size_;
    std::unique_ptr<std::uint8_t[]> data_;

public:
    serialization_buffer(std::size_t size) : size_(size), data_(new std::uint8_t[size + 8])
    {
        std::memset(data_.get() + size, 0xde, 8);  // buffer overrun detector
    }
    span<std::uint8_t> to_span() noexcept { return {data(), size()}; }
    operator span<std::uint8_t>() noexcept { return to_span(); }
    std::uint8_t* data() noexcept { return data_.get(); }
    std::size_t size() const noexcept { return size_; }
    void check(span<const std::uint8_t> expected) const
    {
        // Check the actual value
        span<const std::uint8_t> actual_populated(data_.get(), size_);
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(expected, actual_populated);

        // Check that we didn't overrun the buffer
        const std::array<std::uint8_t, 8> expected_clean{0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde};
        span<const std::uint8_t> actual_clean(data_.get() + size_, 8);
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(expected_clean, actual_clean);
    }
};

// A special buffer for deserialization tests. Allocates the exact size of the serialized message (contrary to
// std::vector), making it easier for sanitizers to detect overruns
class deserialization_buffer
{
    std::size_t size_;
    std::unique_ptr<std::uint8_t[]> data_;

public:
    explicit deserialization_buffer(std::size_t size) : size_(size), data_(new std::uint8_t[size]) {}
    deserialization_buffer(std::size_t size, std::uint8_t value) : deserialization_buffer(size)
    {
        std::memset(data(), value, size);
    }
    explicit deserialization_buffer(span<const std::uint8_t> data)
        : size_(data.size()), data_(new std::uint8_t[data.size()])
    {
        std::memcpy(data_.get(), data.data(), data.size());
    }
    deserialization_buffer(std::initializer_list<std::uint8_t> data)
        : size_(data.size()), data_(new std::uint8_t[data.size()])
    {
        std::copy(data.begin(), data.end(), data_.get());
    }
    span<const std::uint8_t> to_span() const noexcept { return {data_.get(), size_}; }
    operator span<const std::uint8_t>() const noexcept { return to_span(); }
    std::uint8_t* data() noexcept { return data_.get(); }
    const std::uint8_t* data() const noexcept { return data_.get(); }
    std::size_t size() const noexcept { return size_; }
};

template <class T>
void do_serialize_test(T value, span<const std::uint8_t> serialized)
{
    // Size
    std::size_t expected_size = serialized.size();
    std::size_t actual_size = detail::get_size(value);
    BOOST_TEST(actual_size == expected_size);

    // Serialize
    serialization_buffer buffer(actual_size);
    detail::serialization_context ctx(buffer.data());
    detail::serialize(ctx, value);

    // Check buffer
    buffer.check(serialized);

    // Check iterator
    BOOST_TEST(ctx.first() == buffer.data() + expected_size, "Iterator not updated correctly");
}

template <class T>
void do_deserialize_test(T value, span<const std::uint8_t> serialized)
{
    deserialization_buffer buffer(serialized);
    auto first = buffer.data();
    auto size = buffer.size();
    detail::deserialization_context ctx(first, size);
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
    deserialization_buffer buffer(serialized.size() + 1);
    std::memcpy(buffer.data(), serialized.data(), serialized.size());
    buffer.data()[serialized.size()] = 0xff;

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
    // Create a new buffer with one less byte
    deserialization_buffer buffer(serialized.subspan(0, serialized.size() - 1));
    detail::deserialization_context ctx(buffer.to_span());

    T value{};
    detail::deserialize_errc err = boost::mysql::detail::deserialize(ctx, value);
    BOOST_TEST(err == detail::deserialize_errc::incomplete_message);
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
