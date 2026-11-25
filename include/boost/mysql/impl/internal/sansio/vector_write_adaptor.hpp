//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_VECTOR_WRITE_ADAPTOR_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_VECTOR_WRITE_ADAPTOR_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/core/span.hpp>

#include <cstddef>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

// Adaptor that exposes a std::vector as a contiguous, append-only sequence,
// satisfying the WriteAdaptor concept required by serialization_context.
//
// Used by pipeline_request to accumulate serialized stages into a single vector.
// Unlike write_adaptor, this exposes the entire vector's data() and size().
class vector_write_adaptor
{
    std::vector<std::uint8_t>& buffer_;

public:
    explicit vector_write_adaptor(std::vector<std::uint8_t>& buffer) noexcept
     : buffer_(buffer) {}

    // Pointer to the start of the vector.
    std::uint8_t* data() noexcept { return buffer_.data(); }
    // Total size of the vector.
    std::size_t size() const noexcept { return buffer_.size(); }

    // Appends data to the vector. Never fails under normal circumstances
    // (std::vector handles its own capacity growth and limits).
    error_code append(span<const std::uint8_t> s)
    {
        buffer_.insert(buffer_.end(), s.begin(), s.end());
        return error_code();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
