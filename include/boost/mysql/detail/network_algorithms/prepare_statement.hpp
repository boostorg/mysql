//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/utility/string_view.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <class Stream>
void prepare_statement(
    channel<Stream>& chan,
    boost::string_view statement,
    statement_base& output,
    error_code& err,
    error_info& info
);

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_prepare_statement(
    channel<Stream>& chan,
    boost::string_view statement,
    statement_base& output,
    error_info& info,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/prepare_statement.hpp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_ */
