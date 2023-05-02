//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_ROW_MESSAGE_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_ROW_MESSAGE_HPP

#include <boost/mysql/field_view.hpp>

#include <boost/mysql/detail/protocol/protocol_types.hpp>

#include <boost/core/span.hpp>

#include <stdexcept>
#include <string>

#include "creation/create_message.hpp"
#include "test_common.hpp"

namespace boost {
namespace mysql {
namespace test {

inline std::vector<std::uint8_t> create_text_row_body_span(boost::span<const field_view> fields)
{
    std::vector<std::uint8_t> res;
    for (field_view f : fields)
    {
        if (f.is_int64() || f.is_uint64())
        {
            auto s = f.is_int64() ? std::to_string(f.get_int64()) : std::to_string(f.get_uint64());
            detail::string_lenenc slenenc{s};
            serialize_to_vector(res, slenenc);
        }
        else if (f.is_string())
        {
            detail::string_lenenc slenenc{f.get_string()};
            serialize_to_vector(res, slenenc);
        }
        else
        {
            throw std::runtime_error("create_text_row_message: type not implemented");
        }
    }
    return res;
}

template <class... Args>
std::vector<std::uint8_t> create_text_row_body(const Args&... args)
{
    return create_text_row_body_span(make_fv_arr(args...));
}

template <class... Args>
std::vector<std::uint8_t> create_text_row_message(std::uint8_t seqnum, const Args&... args)
{
    return create_message(seqnum, create_text_row_body(args...));
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
