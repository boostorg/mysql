//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_ROW_MESSAGE_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_ROW_MESSAGE_HPP

#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include "test_common/create_basic.hpp"
#include "test_unit/create_frame.hpp"

namespace boost {
namespace mysql {
namespace test {

inline std::vector<std::uint8_t> serialize_text_row_impl(span<const field_view> fields)
{
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, detail::disable_framing);
    for (field_view f : fields)
    {
        std::string s;
        switch (f.kind())
        {
        case field_kind::int64: s = std::to_string(f.get_int64()); break;
        case field_kind::uint64: s = std::to_string(f.get_uint64()); break;
        case field_kind::float_: s = std::to_string(f.get_float()); break;
        case field_kind::double_: s = std::to_string(f.get_double()); break;
        case field_kind::string: s = f.get_string(); break;
        case field_kind::blob: s.assign(f.get_blob().begin(), f.get_blob().end()); break;
        case field_kind::null: ctx.add(std::uint8_t(0xfb)); continue;
        default: throw std::runtime_error("create_text_row_message: type not implemented");
        }
        detail::string_lenenc{s}.serialize(ctx);
    }
    return buff;
}

template <class... Args>
std::vector<std::uint8_t> create_text_row_body(const Args&... args)
{
    return serialize_text_row_impl(make_fv_arr(args...));
}

template <class... Args>
std::vector<std::uint8_t> create_text_row_message(std::uint8_t seqnum, const Args&... args)
{
    return create_frame(seqnum, create_text_row_body(args...));
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
