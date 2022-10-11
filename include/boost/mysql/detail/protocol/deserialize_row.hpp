//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_DESERIALIZE_ROW_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_DESERIALIZE_ROW_HPP

#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/detail/protocol/serialization_context.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/resultset_base.hpp>

#include <boost/asio/buffer.hpp>

#include <vector>

namespace boost {
namespace mysql {
namespace detail {

// Exposed here for the sake of testing
inline void deserialize_row(
    resultset_encoding encoding,
    deserialization_context& ctx,
    const std::vector<metadata>& meta,
    const std::uint8_t* buffer_first,
    std::vector<field_view>& output,
    error_code& err
);

inline void deserialize_row(
    boost::asio::const_buffer read_message,
    capabilities current_capabilities,
    const std::uint8_t* buffer_first,  // to store strings as offsets and allow buffer reallocation
    resultset_base& result,            // should be valid() and !complete()
    std::vector<field_view>& output,
    error_code& err,
    error_info& info
);

inline void offsets_to_string_views(
    std::vector<field_view>& fields,
    const std::uint8_t* buffer_first
) noexcept
{
    for (auto& f : fields)
        f.offset_to_string_view(buffer_first);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/protocol/impl/deserialize_row.ipp>

#endif
