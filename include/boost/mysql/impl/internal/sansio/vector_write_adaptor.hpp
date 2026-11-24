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

class vector_write_adaptor
{
    std::vector<std::uint8_t>& buffer_;

public:
    explicit vector_write_adaptor(std::vector<std::uint8_t>& buffer)
     : buffer_(buffer) {}

    std::uint8_t* data() noexcept { return buffer_.data(); }
    std::size_t size() const noexcept { return buffer_.size(); }

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
