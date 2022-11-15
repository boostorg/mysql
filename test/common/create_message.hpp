//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATE_MESSAGE_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATE_MESSAGE_HPP

#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/serialization.hpp>
#include <boost/mysql/detail/protocol/serialization_context.hpp>

#include <cstdint>
#include <cstring>
#include <cassert>

#include "buffer_concat.hpp"

namespace boost {
namespace mysql {
namespace test {

inline std::vector<std::uint8_t> create_message(std::uint8_t seqnum, std::vector<std::uint8_t> body)
{
    assert(body.size() <= std::numeric_limits<std::uint32_t>::max());
    auto body_size = static_cast<std::uint32_t>(body.size());
    boost::mysql::detail::packet_header header{boost::mysql::detail::int3{body_size}, seqnum};
    body.resize(body_size + 4);
    std::memmove(body.data() + 4, body.data(), body_size);
    boost::mysql::detail::serialization_context ctx(
        boost::mysql::detail::capabilities(),
        body.data()
    );
    boost::mysql::detail::serialize(ctx, header);
    return body;
}

inline std::vector<std::uint8_t> create_message(
    std::uint8_t seqnum1,
    std::vector<std::uint8_t> body1,
    std::uint8_t seqnum2,
    std::vector<std::uint8_t> body2
)
{
    return concat_copy(
        create_message(seqnum1, std::move(body1)),
        create_message(seqnum2, std::move(body2))
    );
}

inline std::vector<std::uint8_t> create_message(
    std::uint8_t seqnum1,
    std::vector<std::uint8_t> body1,
    std::uint8_t seqnum2,
    std::vector<std::uint8_t> body2,
    std::uint8_t seqnum3,
    std::vector<std::uint8_t> body3
)
{
    return concat_copy(
        create_message(seqnum1, std::move(body1)),
        create_message(seqnum2, std::move(body2)),
        create_message(seqnum3, std::move(body3))
    );
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
