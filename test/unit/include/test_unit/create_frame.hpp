//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_MESSAGE_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_MESSAGE_HPP

#include <boost/mysql/string_view.hpp>

#include <cassert>
#include <cstdint>
#include <cstring>

#include "protocol/protocol.hpp"
#include "protocol/serialization.hpp"

namespace boost {
namespace mysql {
namespace test {

inline std::vector<std::uint8_t> create_frame(std::uint8_t seqnum, span<const std::uint8_t> body)
{
    BOOST_ASSERT(body.size() <= 0xffffff);  // it should fit in a single frame
    std::vector<std::uint8_t> res(body.size() + 4);
    detail::frame_header header{static_cast<std::uint32_t>(body.size()), seqnum};
    detail::serialize_frame_header(header, span<std::uint8_t, detail::frame_header_size>{res.data(), 4u});
    std::memcpy(res.data() + 4, body.data(), body.size());
    return res;
}

template <class... Args>
void serialize_to_vector(std::vector<std::uint8_t>& res, const Args&... args)
{
    std::size_t size = detail::get_size(args...);
    std::size_t old_size = res.size();
    res.resize(old_size + size);
    detail::serialization_context ctx(res.data() + old_size);
    detail::serialize(ctx, args...);
}

template <class... Args>
std::vector<std::uint8_t> serialize_to_vector(const Args&... args)
{
    std::vector<std::uint8_t> res;
    serialize_to_vector(res, args...);
    return res;
}

// inline std::vector<std::uint8_t> create_message(
//     std::uint8_t seqnum1,
//     std::vector<std::uint8_t> body1,
//     std::uint8_t seqnum2,
//     std::vector<std::uint8_t> body2
// )
// {
//     return concat_copy(create_message(seqnum1, std::move(body1)), create_message(seqnum2,
//     std::move(body2)));
// }

// inline std::vector<std::uint8_t> create_message(
//     std::uint8_t seqnum1,
//     std::vector<std::uint8_t> body1,
//     std::uint8_t seqnum2,
//     std::vector<std::uint8_t> body2,
//     std::uint8_t seqnum3,
//     std::vector<std::uint8_t> body3
// )
// {
//     return concat_copy(
//         create_message(seqnum1, std::move(body1)),
//         create_message(seqnum2, std::move(body2)),
//         create_message(seqnum3, std::move(body3))
//     );
// }

// class ok_msg_builder
// {
//     ok_builder impl_;
//     std::uint8_t seqnum_{};

// public:
//     ok_msg_builder() = default;
//     ok_msg_builder seqnum(std::uint8_t v)
//     {
//         seqnum_ = v;
//         return *this;
//     }
//     ok_msg_builder& affected_rows(std::uint64_t v)
//     {
//         impl_.affected_rows(v);
//         return *this;
//     }
//     ok_msg_builder& last_insert_id(std::uint64_t v)
//     {
//         impl_.last_insert_id(v);
//         return *this;
//     }
//     ok_msg_builder& warnings(std::uint16_t v)
//     {
//         impl_.warnings(v);
//         return *this;
//     }
//     ok_msg_builder& info(string_view v)
//     {
//         impl_.info(v);
//         return *this;
//     }
//     ok_msg_builder& more_results(bool v)
//     {
//         impl_.more_results(v);
//         return *this;
//     }

//     std::vector<std::uint8_t> build_body(std::uint8_t header = 0x00)
//     {
//         auto pack = impl_.build();
//         auto res = serialize_to_vector(
//             std::uint8_t(header),
//             pack.affected_rows,
//             pack.last_insert_id,
//             pack.status_flags,
//             pack.warnings
//         );
//         // When info is empty, it's actually omitted in the ok_packet
//         if (!pack.info.value.empty())
//         {
//             auto vinfo = serialize_to_vector(pack.info);
//             concat(res, vinfo);
//         }
//         return res;
//     }

//     std::vector<std::uint8_t> build_ok() { return create_message(seqnum_, build_body(0)); }

//     std::vector<std::uint8_t> build_eof() { return create_message(seqnum_, build_body(0xfe)); }
// };

// inline std::vector<std::uint8_t> create_coldef_message(
//     std::uint8_t seqnum,
//     const detail::column_definition_packet& pack
// )
// {
//     return create_message(
//         seqnum,
//         serialize_to_vector(
//             pack.catalog,
//             pack.schema,
//             pack.table,
//             pack.org_table,
//             pack.name,
//             pack.org_name,
//             boost::mysql::detail::int_lenenc(0x0c),  // length of fixed fields
//             pack.character_set,
//             pack.column_length,
//             pack.type,
//             pack.flags,
//             pack.decimals,
//             std::uint16_t(0)  // padding
//         )
//     );
// }

// inline std::vector<std::uint8_t> create_coldef_message(
//     std::uint8_t seqnum,
//     detail::protocol_field_type type,
//     string_view name = "mycol"
// )
// {
//     return create_coldef_message(seqnum, create_coldef(type, name));
// }

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
