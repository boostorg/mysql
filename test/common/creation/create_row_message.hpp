//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_ROW_MESSAGE_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_ROW_MESSAGE_HPP

#include <boost/mysql/field_view.hpp>

#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
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
        std::string s;
        switch (f.kind())
        {
        case field_kind::int64: s = std::to_string(f.get_int64()); break;
        case field_kind::uint64: s = std::to_string(f.get_uint64()); break;
        case field_kind::float_: s = std::to_string(f.get_float()); break;
        case field_kind::double_: s = std::to_string(f.get_double()); break;
        case field_kind::string: s = f.get_string(); break;
        case field_kind::null: serialize_to_vector(res, std::uint8_t(0xfb)); continue;
        default: throw std::runtime_error("create_text_row_message: type not implemented");
        }
        detail::string_lenenc slenenc{s};
        serialize_to_vector(res, slenenc);
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

// Helper to run execution_processor tests, since these expect long-lived row buffers
class rowbuff
{
    std::vector<std::uint8_t> data_;

public:
    template <class... Args>
    rowbuff(const Args&... args) : data_(create_text_row_body(args...))
    {
    }

    // Useful for tests that need invalid row bodies
    std::vector<std::uint8_t>& data() noexcept { return data_; }

    detail::deserialization_context ctx() noexcept
    {
        return detail::deserialization_context(
            data_.data(),
            data_.data() + data_.size(),
            detail::capabilities()
        );
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
