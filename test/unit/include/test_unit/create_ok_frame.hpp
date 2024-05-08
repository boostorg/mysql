//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_OK_FRAME_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_OK_FRAME_HPP

#include <boost/mysql/detail/ok_view.hpp>

#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include "test_unit/create_frame.hpp"

namespace boost {
namespace mysql {
namespace test {

inline std::vector<std::uint8_t> serialize_ok_impl(const detail::ok_view& pack, std::uint8_t header)
{
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, detail::disable_framing);

    ctx.serialize(
        detail::int1{header},
        detail::int_lenenc{pack.affected_rows},
        detail::int_lenenc{pack.last_insert_id},
        detail::int2{pack.status_flags},
        detail::int2{pack.warnings}
    );
    // When info is empty, it's actually omitted in the ok_packet
    if (!pack.info.empty())
    {
        detail::string_lenenc{pack.info}.serialize(ctx);
    }
    return buff;
}

inline std::vector<std::uint8_t> create_ok_body(const detail::ok_view& ok)
{
    return serialize_ok_impl(ok, 0x00);
}

inline std::vector<std::uint8_t> create_eof_body(const detail::ok_view& ok)
{
    return serialize_ok_impl(ok, 0xfe);
}

inline std::vector<std::uint8_t> create_ok_frame(std::uint8_t seqnum, const detail::ok_view& ok)
{
    return create_frame(seqnum, create_ok_body(ok));
}

inline std::vector<std::uint8_t> create_eof_frame(std::uint8_t seqnum, const detail::ok_view& ok)
{
    return create_frame(seqnum, create_eof_body(ok));
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
