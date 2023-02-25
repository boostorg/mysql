//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_QUERY_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_QUERY_HPP

#pragma once

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/network_algorithms/execute.hpp>
#include <boost/mysql/detail/network_algorithms/execute_query.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution.hpp>

#include <memory>

namespace boost {
namespace mysql {
namespace detail {

class query_execution_request : public execution_request
{
    string_view query_;

public:
    query_execution_request(string_view query) noexcept : query_(query) {}
    void serialize(capabilities caps, std::vector<std::uint8_t>& buffer) const override
    {
        com_query_packet request{string_eof(query_)};
        serialize_message(request, caps, buffer);
    }
    resultset_encoding encoding() const override { return resultset_encoding::text; }

    static std::unique_ptr<execution_request> create(string_view query)
    {
        return std::unique_ptr<execution_request>(new query_execution_request(query));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
void boost::mysql::detail::start_query(
    channel<Stream>& channel,
    string_view query,
    execution_state& output,
    error_code& err,
    diagnostics& diag
)
{
    start_execution(channel, false, query_execution_request(query), output.impl_, err, diag);
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_query(
    channel<Stream>& chan,
    string_view query,
    execution_state& output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return async_start_execution(
        chan,
        false,
        query_execution_request::create(query),
        output.impl_,
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
void boost::mysql::detail::query(
    channel<Stream>& channel,
    string_view query,
    results& output,
    error_code& err,
    diagnostics& diag
)
{
    execute(channel, query_execution_request(query), results_access::get_state(output), err, diag);
}

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_query(
    channel<Stream>& chan,
    string_view query,
    results& output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return async_execute(
        chan,
        std::unique_ptr<execution_request>(new query_execution_request(query)),
        results_access::get_state(output),
        diag,
        std::forward<CompletionToken>(token)
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
