//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_SERIALIZATION_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_SERIALIZATION_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/protocol/basic_types.hpp>
#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/constants.hpp>
#include <boost/mysql/impl/internal/protocol/protocol_field_type.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/endian/detail/endian_load.hpp>
#include <boost/endian/detail/endian_store.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

//
// helpers
//
inline string_view to_string(span<const std::uint8_t> v) noexcept
{
    return string_view(reinterpret_cast<const char*>(v.data()), v.size());
}
inline span<const std::uint8_t> to_span(string_view v) noexcept
{
    return span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(v.data()), v.size());
}

struct frame_header_packet
{
    int3 packet_size;
    std::uint8_t sequence_number;
};

class serialization_context
{
    std::vector<std::uint8_t>& buffer_;
    std::size_t max_frame_size_;
    std::size_t next_frame_header_;

public:
    serialization_context(std::vector<std::uint8_t>& buff, std::size_t max_frame_size = max_packet_size)
        : buffer_(buff),
          max_frame_size_(max_frame_size),
          next_frame_header_(buff.size() + max_frame_size_ + frame_header_size)
    {
        // Add space for the initial header. TODO: do we want to keep this in the constructor?
        buffer_.resize(buffer_.size() + frame_header_size);
    }

    span<std::uint8_t> reserve_space(std::size_t size)
    {
        std::size_t offset = buffer_.size();
        buffer_.resize(offset + size);
        return {buffer_.data() + offset, size};
    }

    void add(span<const std::uint8_t> content)
    {
        buffer_.insert(buffer_.end(), content.begin(), content.end());
    }

    void add(std::uint8_t value) { buffer_.push_back(value); }

    void add_checked(span<const std::uint8_t> content)
    {
        // Add any required frame headers we didn't add until now
        add_frame_headers();

        // Add the content in chunks, inserting space for headers where required
        std::size_t content_offset = 0;
        while (content_offset < content.size())
        {
            // Serialize what we've got space for
            BOOST_ASSERT(next_frame_header_ > buffer_.size());
            auto remaining_content = static_cast<std::size_t>(content.size() - content_offset);
            auto remaining_frame = static_cast<std::size_t>(next_frame_header_ - buffer_.size());
            auto size_to_write = (std::min)(remaining_content, remaining_frame);
            buffer_.insert(
                buffer_.end(),
                content.data() + content_offset,
                content.data() + content_offset + size_to_write
            );
            content_offset += size_to_write;

            // Insert space for a frame header if required
            if (buffer_.size() == next_frame_header_)
            {
                buffer_.resize(buffer_.size() + 4);
                next_frame_header_ += (max_frame_size_ + frame_header_size);
            }
        }
    }

    void add_frame_headers()
    {
        while (next_frame_header_ <= buffer_.size())
        {
            // Insert space for the frame header where needed
            const std::array<std::uint8_t, frame_header_size> placeholder{};
            buffer_.insert(buffer_.begin() + next_frame_header_, placeholder.begin(), placeholder.end());

            // Update the next frame header offset
            next_frame_header_ += (max_frame_size_ + frame_header_size);
        }
    }
};

// Integers
template <class T, class = typename std::enable_if<std::is_integral<T>::value>::type>
void serialize_to(span<std::uint8_t, sizeof(T)> to, T input)
{
    endian::endian_store<T, sizeof(T), endian::order::little>(to.data(), input);
}

template <class T, class = typename std::enable_if<std::is_integral<T>::value>::type>
void serialize(serialization_context& ctx, T input)
{
    std::array<std::uint8_t, sizeof(T)> buffer{};
    serialize_to(buffer, input);
    ctx.add(buffer);
}

inline void serialize_to(span<std::uint8_t, 3> to, int3 input)
{
    endian::store_little_u24(to.data(), input.value);
}

inline void serialize(serialization_context& ctx, int3 input)
{
    std::array<std::uint8_t, 3> buffer;
    serialize_to(buffer, input);
    ctx.add(buffer);
}

// protocol_field_type
inline void serialize(serialization_context& ctx, protocol_field_type input)
{
    static_assert(
        std::is_same<typename std::underlying_type<protocol_field_type>::type, std::uint8_t>::value,
        "Internal error"
    );
    ctx.add(static_cast<std::uint8_t>(input));
}

// string_fixed
template <std::size_t N>
void serialize(serialization_context& ctx, string_fixed<N> input)
{
    ctx.add({reinterpret_cast<const std::uint8_t*>(input.value.data()), N});
}

// int_lenenc
inline void serialize(serialization_context& ctx, int_lenenc input)
{
    if (input.value < 251)
    {
        ctx.add(static_cast<std::uint8_t>(input.value));
    }
    else if (input.value < 0x10000)
    {
        ctx.add(static_cast<std::uint8_t>(0xfc));
        serialize(ctx, static_cast<std::uint16_t>(input.value));
    }
    else if (input.value < 0x1000000)
    {
        ctx.add(static_cast<std::uint8_t>(0xfd));
        serialize(ctx, int3{static_cast<std::uint32_t>(input.value)});
    }
    else
    {
        ctx.add(static_cast<std::uint8_t>(0xfe));
        serialize(ctx, static_cast<std::uint64_t>(input.value));
    }
}

// string_null
inline void serialize(serialization_context& ctx, string_null input)
{
    ctx.add(to_span(input.value));
    ctx.add(static_cast<std::uint8_t>(0));  // null terminator
}

// string_eof
inline void serialize(serialization_context& ctx, string_eof input) { ctx.add(to_span(input.value)); }
inline void serialize_checked(serialization_context& ctx, string_eof input)
{
    ctx.add_checked(to_span(input.value));
}

// string_lenenc
inline void serialize(serialization_context& ctx, string_lenenc input)
{
    serialize(ctx, int_lenenc{input.value.size()});
    ctx.add(to_span(input.value));
}
inline void serialize_checked(serialization_context& ctx, string_lenenc input)
{
    serialize(ctx, int_lenenc{input.value.size()});
    ctx.add_checked(to_span(input.value));
}

// compound types
template <class FirstType, class SecondType, class... Rest>
void serialize(
    serialization_context& ctx,
    const FirstType& first,
    const SecondType& second,
    const Rest&... rest
) noexcept
{
    serialize(ctx, first);
    serialize(ctx, second, rest...);
}

// frame header
inline void serialize_to(span<std::uint8_t, frame_header_size> to, frame_header_packet input)
{
    serialize_to(span<std::uint8_t, 3>(to.data(), 3), input.packet_size);
    to[3] = input.sequence_number;
}

// Write frame headers to an already serialized message with space for them
inline std::uint8_t write_frame_headers(
    span<std::uint8_t> buffer,
    std::uint8_t seqnum,
    std::size_t max_frame_size
)
{
    std::size_t offset = 0;
    while (offset < buffer.size())
    {
        // Calculate the current frame size
        std::size_t frame_first = offset + frame_header_size;
        std::size_t frame_last = (std::min)(frame_first + max_frame_size, buffer.size());
        auto frame_size = static_cast<std::uint32_t>(frame_last - frame_first);

        // Compose the header
        frame_header_packet header{int3{frame_size}, seqnum++};

        // Write the frame header
        BOOST_ASSERT(frame_first <= buffer.size());
        serialize_to(
            span<std::uint8_t, frame_header_size>(buffer.data() + offset, frame_header_size),
            header
        );

        // Skip to the next frame
        offset = frame_last;
    }

    // We should have finished just at the buffer end
    BOOST_ASSERT(offset == buffer.size());

    return seqnum;
}

//
// deserialization
//

// We operate with this enum directly in the deserialization routines for efficiency, then transform it to an
// actual error code
enum class deserialize_errc
{
    ok = 0,
    incomplete_message = 1,
    protocol_value_error,
    server_unsupported
};

inline error_code to_error_code(deserialize_errc v)
{
    switch (v)
    {
    case deserialize_errc::ok: return error_code();
    case deserialize_errc::incomplete_message: return error_code(client_errc::incomplete_message);
    case deserialize_errc::protocol_value_error: return error_code(client_errc::protocol_value_error);
    case deserialize_errc::server_unsupported: return error_code(client_errc::server_unsupported);
    default: BOOST_ASSERT(false); return error_code();  // avoid warnings
    }
}

class deserialization_context
{
    const std::uint8_t* first_;
    const std::uint8_t* last_;

public:
    deserialization_context(span<const std::uint8_t> data) noexcept
        : deserialization_context(data.data(), data.size())
    {
    }
    deserialization_context(const std::uint8_t* first, std::size_t size) noexcept
        : first_(first), last_(first + size){};
    const std::uint8_t* first() const { return first_; }
    const std::uint8_t* last() const { return last_; }
    void advance(std::size_t sz)
    {
        first_ += sz;
        BOOST_ASSERT(last_ >= first_);
    }
    void rewind(std::size_t sz) { first_ -= sz; }
    std::size_t size() const { return last_ - first_; }
    bool empty() const { return last_ == first_; }
    bool enough_size(std::size_t required_size) const { return size() >= required_size; }
    deserialize_errc copy(void* to, std::size_t sz)
    {
        if (!enough_size(sz))
            return deserialize_errc::incomplete_message;
        memcpy(to, first_, sz);
        advance(sz);
        return deserialize_errc::ok;
    }
    string_view get_string(std::size_t sz) const
    {
        return string_view(reinterpret_cast<const char*>(first_), sz);
    }
    error_code check_extra_bytes() const
    {
        return empty() ? error_code() : error_code(client_errc::extra_bytes);
    }
    span<const std::uint8_t> to_span() const { return span<const std::uint8_t>(first_, size()); }
};

// integers
template <class T, class = typename std::enable_if<std::is_integral<T>::value>::type>
deserialize_errc deserialize(deserialization_context& ctx, T& output)
{
    constexpr std::size_t sz = sizeof(T);
    if (!ctx.enough_size(sz))
    {
        return deserialize_errc::incomplete_message;
    }
    output = endian::endian_load<T, sz, boost::endian::order::little>(ctx.first());
    ctx.advance(sz);
    return deserialize_errc::ok;
}

// int3
inline deserialize_errc deserialize(deserialization_context& ctx, int3& output)
{
    if (!ctx.enough_size(3))
        return deserialize_errc::incomplete_message;
    output.value = endian::load_little_u24(ctx.first());
    ctx.advance(3);
    return deserialize_errc::ok;
}

// int_lenenc
inline deserialize_errc deserialize(deserialization_context& ctx, int_lenenc& output)
{
    std::uint8_t first_byte = 0;
    auto err = deserialize(ctx, first_byte);
    if (err != deserialize_errc::ok)
    {
        return err;
    }

    if (first_byte == 0xFC)
    {
        std::uint16_t value = 0;
        err = deserialize(ctx, value);
        output.value = value;
    }
    else if (first_byte == 0xFD)
    {
        int3 value{};
        err = deserialize(ctx, value);
        output.value = value.value;
    }
    else if (first_byte == 0xFE)
    {
        std::uint64_t value = 0;
        err = deserialize(ctx, value);
        output.value = value;
    }
    else
    {
        err = deserialize_errc::ok;
        output.value = first_byte;
    }
    return err;
}

// protocol_field_type
inline deserialize_errc deserialize(deserialization_context& ctx, protocol_field_type& output)
{
    std::underlying_type<protocol_field_type>::type value = 0;
    auto err = deserialize(ctx, value);
    output = static_cast<protocol_field_type>(value);
    return err;
}

// string_fixed
template <std::size_t N>
deserialize_errc deserialize(deserialization_context& ctx, string_fixed<N>& output)
{
    if (!ctx.enough_size(N))
        return deserialize_errc::incomplete_message;
    memcpy(output.value.data(), ctx.first(), N);
    ctx.advance(N);
    return deserialize_errc::ok;
}

// string_null
inline deserialize_errc deserialize(deserialization_context& ctx, string_null& output)
{
    auto string_end = std::find(ctx.first(), ctx.last(), 0);
    if (string_end == ctx.last())
    {
        return deserialize_errc::incomplete_message;
    }
    std::size_t length = string_end - ctx.first();
    output.value = ctx.get_string(length);
    ctx.advance(length + 1);  // skip the null terminator
    return deserialize_errc::ok;
}

// string_eof
inline deserialize_errc deserialize(deserialization_context& ctx, string_eof& output)
{
    std::size_t size = ctx.size();
    output.value = ctx.get_string(size);
    ctx.advance(size);
    return deserialize_errc::ok;
}

// string_lenenc
inline deserialize_errc deserialize(deserialization_context& ctx, string_lenenc& output)
{
    int_lenenc length;
    auto err = deserialize(ctx, length);
    if (err != deserialize_errc::ok)
    {
        return err;
    }
    if (length.value > (std::numeric_limits<std::size_t>::max)())
    {
        return deserialize_errc::protocol_value_error;
    }
    auto len = static_cast<std::size_t>(length.value);
    if (!ctx.enough_size(len))
    {
        return deserialize_errc::incomplete_message;
    }

    output.value = ctx.get_string(len);
    ctx.advance(len);
    return deserialize_errc::ok;
}

// compound types
template <class FirstType, class SecondType, class... Rest>
deserialize_errc deserialize(
    deserialization_context& ctx,
    FirstType& first,
    SecondType& second,
    Rest&... tail
) noexcept
{
    deserialize_errc err = deserialize(ctx, first);
    if (err == deserialize_errc::ok)
    {
        err = deserialize(ctx, second, tail...);
    }
    return err;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
