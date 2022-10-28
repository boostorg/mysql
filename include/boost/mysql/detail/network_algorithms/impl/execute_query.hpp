//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP

#pragma once

#include <boost/mysql/detail/network_algorithms/execute_generic.hpp>
#include <boost/mysql/detail/network_algorithms/execute_query.hpp>
#include <boost/mysql/detail/protocol/query_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/utility/string_view_fwd.hpp>

namespace boost {
namespace mysql {
namespace detail {

class query_request_maker
{
    boost::string_view query_;

public:
    struct storage_type
    {
    };

    query_request_maker(boost::string_view query) noexcept : query_(query) {}
    storage_type make_storage() const noexcept { return storage_type(); }
    com_query_packet make_request(storage_type) const noexcept
    {
        return com_query_packet{string_eof(query_)};
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
void boost::mysql::detail::execute_query(
    channel<Stream>& channel,
    boost::string_view query,
    resultset_base& output,
    error_code& err,
    error_info& info
)
{
    execute_generic(
        resultset_encoding::text,
        channel,
        com_query_packet{string_eof(query)},
        output,
        err,
        info
    );
}

template <class Stream, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute_query(
    channel<Stream>& chan,
    boost::string_view query,
    resultset_base& output,
    error_info& info,
    CompletionToken&& token
)
{
    return async_execute_generic(
        resultset_encoding::text,
        chan,
        query_request_maker(query),
        output,
        info,
        std::forward<CompletionToken>(token)
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
