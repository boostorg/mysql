//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_ROW_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_ROW_HPP

#include "boost/mysql/detail/network_algorithms/common.hpp"
#include "boost/mysql/metadata.hpp"
#include <string_view>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

enum class read_row_result
{
    error,
    row,
    eof
};

template <typename StreamType>
read_row_result read_row(
    deserialize_row_fn deserializer,
    channel<StreamType>& channel,
    const std::vector<field_metadata>& meta,
    bytestring& buffer,
    std::vector<value>& output_values,
    ok_packet& output_ok_packet,
    error_code& err,
    error_info& info
);

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, read_row_result))
async_read_row(
    deserialize_row_fn deserializer,
    channel<StreamType>& channel,
    const std::vector<field_metadata>& meta,
    bytestring& buffer,
    std::vector<value>& output_values,
    ok_packet& output_ok_packet,
    CompletionToken&& token,
    error_info& output_info
);


} // detail
} // mysql
} // boost

#include "boost/mysql/detail/network_algorithms/impl/read_row.hpp"



#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_HPP_ */
