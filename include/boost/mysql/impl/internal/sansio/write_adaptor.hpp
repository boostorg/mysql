//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_WRITE_ADAPTOR_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_WRITE_ADAPTOR_HPP

#include <boost/mysql/impl/internal/sansio/rw_buffer.hpp>

#include <boost/config.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstring> // std::memcpy

namespace boost {
namespace mysql {
namespace detail {

// Suppress false positive on GCC 10+
#if defined(BOOST_GCC) && BOOST_GCC >= 100000
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
class write_adaptor
{
    rw_buffer& buffer_;
    std::size_t offset_{0};

public:
    explicit write_adaptor(rw_buffer& buffer)
     : buffer_(buffer) {}

    std::uint8_t* data() noexcept { return buffer_.free_first(); }
    std::size_t size() const noexcept { return offset_; }

    error_code append(span<const std::uint8_t> s)
    {
        if (s.empty())
            return error_code();

        std::size_t count = s.size();
        std::size_t new_offset = offset_ + count;
        auto ec = buffer_.grow_to_fit(new_offset);
        if (ec)
            return ec;
        std::memcpy(data() + offset_, s.data(), count);
        offset_ = new_offset;

        return error_code();
    }
};
#if defined(BOOST_GCC) && BOOST_GCC >= 100000
#pragma GCC diagnostic pop
#endif

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
