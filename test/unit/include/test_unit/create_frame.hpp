//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_FRAME_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_FRAME_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/protocol/frame_header.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include <boost/core/span.hpp>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "test_common/buffer_concat.hpp"

namespace boost {
namespace mysql {
namespace test {

inline std::vector<std::uint8_t> create_frame(std::uint8_t seqnum, span<const std::uint8_t> body)
{
    BOOST_ASSERT(body.size() <= 0xffffff);  // it should fit in a single frame
    std::vector<std::uint8_t> res(detail::frame_header_size);
    detail::serialize_frame_header(
        span<std::uint8_t, detail::frame_header_size>{res.data(), detail::frame_header_size},
        detail::frame_header{static_cast<std::uint32_t>(body.size()), seqnum}
    );
    concat(res, body);
    return res;
}

inline std::vector<std::uint8_t> create_frame(std::uint8_t seqnum, const std::vector<std::uint8_t>& body)
{
    return create_frame(seqnum, boost::span<const std::uint8_t>(body));
}

inline std::vector<std::uint8_t> create_empty_frame(std::uint8_t seqnum)
{
    return create_frame(seqnum, boost::span<const std::uint8_t>());
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
