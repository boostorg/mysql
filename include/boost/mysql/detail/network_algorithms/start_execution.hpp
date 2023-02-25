//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_START_EXECUTION_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_START_EXECUTION_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>

#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/db_flavor.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/protocol/query_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

class execution_request
{
public:
    virtual ~execution_request() {}
    virtual void serialize(capabilities, std::vector<std::uint8_t>& buffer) const = 0;
    virtual resultset_encoding encoding() const = 0;
};

template <class Stream>
void start_execution(
    channel<Stream>& channel,
    error_code fast_fail,
    bool append_mode,
    const execution_request& req,
    execution_state_impl& st,
    error_code& err,
    diagnostics& diag
);

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_start_execution(
    channel<Stream>& chan,
    error_code fast_fail,
    bool append_mode,
    std::unique_ptr<execution_request> req,  // TODO: find a better way to perform this erasing
    execution_state_impl& st,
    diagnostics& diag,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/start_execution.hpp>

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_ */
