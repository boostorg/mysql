//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP

#pragma once

#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/detail/network_algorithms/execute.hpp>
#include <boost/mysql/detail/network_algorithms/execute_statement.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
com_stmt_execute_packet<FieldViewFwdIterator> make_stmt_execute_packet(
    std::uint32_t stmt_id,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last
) noexcept(std::is_nothrow_copy_constructible<FieldViewFwdIterator>::value)
{
    return com_stmt_execute_packet<FieldViewFwdIterator>{
        stmt_id,
        std::uint8_t(0),   // flags
        std::uint32_t(1),  // iteration count
        std::uint8_t(1),   // new params flag: set
        params_first,
        params_last,
    };
}

template <BOOST_MYSQL_FIELD_LIKE... T, std::size_t... I>
std::array<field_view, sizeof...(T)> tuple_to_array_impl(const std::tuple<T...>& t, boost::mp11::index_sequence<I...>) noexcept
{
    return std::array<field_view, sizeof...(T)>{{field_view(std::get<I>(t))...}};
}

template <BOOST_MYSQL_FIELD_LIKE... T>
std::array<field_view, sizeof...(T)> tuple_to_array(const std::tuple<T...>& t) noexcept
{
    return tuple_to_array_impl(t, boost::mp11::make_index_sequence<sizeof...(T)>());
}

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
class stmt_it_execution_request : public execution_request
{
    std::uint32_t stmt_id_;
    FieldViewFwdIterator first_;
    FieldViewFwdIterator last_;

public:
    stmt_it_execution_request(std::uint32_t stmt_id, FieldViewFwdIterator first, FieldViewFwdIterator last)
        : stmt_id_(stmt_id), first_(first), last_(last)
    {
    }

    void serialize(capabilities caps, std::vector<std::uint8_t>& buffer) const override
    {
        auto request = make_stmt_execute_packet(stmt_id_, first_, last_);
        serialize_message(request, caps, buffer);
    }
    resultset_encoding encoding() const override { return resultset_encoding::binary; }
};

template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
class stmt_tuple_execution_request : public execution_request
{
    std::uint32_t stmt_id_;
    FieldLikeTuple params_;

public:
    // We need a deduced context to enable perfect forwarding
    template <BOOST_MYSQL_FIELD_LIKE_TUPLE DeducedTuple>
    stmt_tuple_execution_request(std::uint32_t stmt_id, DeducedTuple&& params)
        : stmt_id_(stmt_id), params_(std::forward<DeducedTuple>(params))
    {
    }

    void serialize(capabilities caps, std::vector<std::uint8_t>& buffer) const override
    {
        auto field_views = tuple_to_array(params_);
        auto request = make_stmt_execute_packet(stmt_id_, field_views.begin(), field_views.end());
        serialize_message(request, caps, buffer);
    }
    resultset_encoding encoding() const override { return resultset_encoding::binary; }
};

inline error_code check_num_params(const statement& stmt, std::size_t param_count)
{
    return param_count == stmt.num_params() ? error_code() : make_error_code(client_errc::wrong_num_params);
}

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
error_code check_num_params(
    const statement& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last
)
{
    return check_num_params(stmt, std::distance(params_first, params_last));
}

template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
error_code check_num_params(const statement& stmt, const FieldLikeTuple&)
{
    return check_num_params(stmt, std::tuple_size<FieldLikeTuple>::value);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
void boost::mysql::detail::execute_statement(
    channel<Stream>& channel,
    const statement& stmt,
    const FieldLikeTuple& params,
    results& output,
    error_code& err,
    diagnostics& diag
)
{
    execute(
        channel,
        check_num_params(stmt, params),
        stmt_tuple_execution_request<FieldLikeTuple>(stmt.id(), params),  // TODO: this is optimizable
        results_access::get_state(output),
        err,
        diag
    );
}

template <
    class Stream,
    BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute_statement(
    channel<Stream>& chan,
    const statement& stmt,
    FieldLikeTuple&& params,
    results& output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    using decayed_tuple = typename std::decay<FieldLikeTuple>::type;
    return async_execute(
        chan,
        check_num_params(stmt, params),
        std::unique_ptr<execution_request>(
            new stmt_tuple_execution_request<decayed_tuple>(std::forward<FieldLikeTuple>(params))
        ),
        results_access::get_state(output),
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream, BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void boost::mysql::detail::start_statement_execution(
    channel<Stream>& chan,
    const statement& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& output,
    error_code& err,
    diagnostics& diag
)
{
    start_execution(
        chan,
        check_num_params(stmt, params_first, params_last),
        false,
        stmt_it_execution_request<FieldViewFwdIterator>(stmt.id(), params_first, params_last),
        output.impl_,
        err,
        diag
    );
}

template <
    class Stream,
    BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_statement_execution(
    channel<Stream>& chan,
    const statement& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    execution_state& output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return async_start_execution(
        chan,
        check_num_params(stmt, params_first, params_last),
        false,
        std::unique_ptr<execution_request>(
            new stmt_it_execution_request<FieldViewFwdIterator>(stmt.id(), params_first, params_last)
        ),
        output.impl_,
        diag,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
void boost::mysql::detail::start_statement_execution(
    channel<Stream>& channel,
    const statement& stmt,
    const FieldLikeTuple& params,
    execution_state& output,
    error_code& err,
    diagnostics& diag
)
{
    auto params_array = tuple_to_array(params);
    start_statement_execution(channel, stmt, params_array.begin(), params_array.end(), output, err, diag);
}

template <
    class Stream,
    BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_start_statement_execution(
    channel<Stream>& chan,
    const statement& stmt,
    FieldLikeTuple&& params,
    execution_state& output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    using decayed_tuple = typename std::decay<FieldLikeTuple>::type;
    return async_start_execution(
        chan,
        check_num_params(stmt, std::tuple_size<decayed_tuple>::value),
        false,
        std::unique_ptr<execution_request>(
            new stmt_tuple_execution_request<decayed_tuple>(std::forward<FieldLikeTuple>(params))
        ),
        output.impl_,
        diag,
        std::forward<CompletionToken>(token)
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_QUERY_HPP_ */
