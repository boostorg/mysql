//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>

namespace boost {
namespace mysql {
namespace detail {

// TODO: rename this
// Exposed for the sake of testing
struct execute_response
{
    enum class type_t
    {
        num_fields,
        ok_packet,
        error
    } type;
    union data_t
    {
        static_assert(std::is_trivially_destructible<error_code>::value, "");

        std::size_t num_fields;
        ok_packet ok_pack;
        error_code err;

        data_t(size_t v) noexcept : num_fields(v) {}
        data_t(const ok_packet& v) noexcept : ok_pack(v) {}
        data_t(error_code v) noexcept : err(v) {}
    } data;

    execute_response(std::size_t v) noexcept : type(type_t::num_fields), data(v) {}
    execute_response(const ok_packet& v) noexcept : type(type_t::ok_packet), data(v) {}
    execute_response(error_code v) noexcept : type(type_t::error), data(v) {}
};
inline execute_response deserialize_execute_response(
    boost::asio::const_buffer msg,
    capabilities caps,
    db_flavor flavor,
    diagnostics& diag
) noexcept;

template <class Stream>
void read_resultset_head(
    channel<Stream>& channel,
    execution_state_impl& st,
    error_code& err,
    diagnostics& diag
);

template <class Stream, BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_read_resultset_head(
    channel<Stream>& channel,
    execution_state_impl& st,
    diagnostics& diag,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/read_resultset_head.hpp>

#endif
