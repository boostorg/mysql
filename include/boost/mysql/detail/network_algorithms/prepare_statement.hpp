//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP

#include "boost/mysql/detail/network_algorithms/common.hpp"
#include "boost/mysql/prepared_statement.hpp"

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType>
void prepare_statement(
    channel<StreamType>& chan,
    std::string_view statement,
    error_code& err,
    error_info& info,
    prepared_statement<StreamType>& output
);

template <typename StreamType>
using prepare_statement_signature = void(error_code, prepared_statement<StreamType>);

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, prepare_statement_signature<StreamType>)
async_prepare_statement(
    channel<StreamType>& chan,
    std::string_view statement,
    CompletionToken&& token,
    error_info* info
);

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/network_algorithms/impl/prepare_statement.hpp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_ */
