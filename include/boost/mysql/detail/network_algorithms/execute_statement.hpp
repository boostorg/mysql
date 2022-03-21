//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP

#include <boost/mysql/detail/network_algorithms/common.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/value.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream, class ValueForwardIterator>
void execute_statement(
    channel<Stream>& channel,
    std::uint32_t statement_id,
    ValueForwardIterator params_begin,
    ValueForwardIterator params_end,
    resultset<Stream>& output,
    error_code& err,
    error_info& info
);

template <class Stream, class ValueForwardIterator, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
async_execute_statement(
    channel<Stream>& chan,
    std::uint32_t statement_id,
    ValueForwardIterator params_begin,
    ValueForwardIterator params_end,
    CompletionToken&& token,
    error_info& info
);

} // detail
} // mysql
} // boost

#include <boost/mysql/detail/network_algorithms/impl/execute_statement.hpp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_STATEMENT_HPP_ */
