//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP

#pragma once

#include <boost/mysql/detail/auxiliar/stringize.hpp>
#include <boost/mysql/detail/network_algorithms/execute_generic.hpp>
#include <boost/mysql/detail/network_algorithms/execute_statement.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/asio/compose.hpp>
#include <boost/asio/post.hpp>
#include <boost/mp11/integer_sequence.hpp>

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

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
class stmt_execute_request_maker_it
{
    std::uint32_t stmt_id_;
    FieldViewFwdIterator first_;
    FieldViewFwdIterator last_;

public:
    struct storage_type
    {
    };

    stmt_execute_request_maker_it(
        std::uint32_t stmt_id,
        FieldViewFwdIterator first,
        FieldViewFwdIterator last
    ) noexcept(std::is_nothrow_copy_constructible<FieldViewFwdIterator>::value)
        : stmt_id_(stmt_id), first_(first), last_(last)
    {
    }

    storage_type make_storage() const noexcept { return storage_type(); }

    com_stmt_execute_packet<FieldViewFwdIterator> make_request(storage_type) const
        noexcept(std::is_nothrow_copy_constructible<FieldViewFwdIterator>::value)
    {
        return make_stmt_execute_packet(stmt_id_, first_, last_);
    }
};

template <BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
class stmt_execute_request_maker_tuple
{
    std::uint32_t stmt_id_;
    FieldLikeTuple params_;

public:
    using storage_type = std::array<field_view, std::tuple_size<FieldLikeTuple>::value>;

    // We need a deduced context to enable perfect forwarding
    template <BOOST_MYSQL_FIELD_LIKE_TUPLE DeducedTuple>
    stmt_execute_request_maker_tuple(std::uint32_t stmt_id, DeducedTuple&& params) noexcept(
        std::is_nothrow_constructible<
            FieldLikeTuple,
            decltype(std::forward<DeducedTuple>(params))>::value
    )
        : stmt_id_(stmt_id), params_(std::forward<DeducedTuple>(params))
    {
    }

    storage_type make_storage() const noexcept { return tuple_to_array(params_); }

    com_stmt_execute_packet<const field_view*> make_request(const storage_type& st) const
    {
        return make_stmt_execute_packet(stmt_id_, st.data(), st.data() + st.size());
    }
};

inline error_code check_num_params(
    const statement_base& stmt,
    std::size_t param_count,
    error_info& info
)
{
    if (param_count != stmt.num_params())
    {
        info.set_message(detail::stringize(
            "execute statement: expected ",
            stmt.num_params(),
            " params, but got ",
            param_count
        ));
        return make_error_code(errc::wrong_num_params);
    }
    else
    {
        return error_code();
    }
}

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
error_code check_num_params(
    const statement_base& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    error_info& info
)
{
    return check_num_params(stmt, std::distance(params_first, params_last), info);
}

struct fast_fail_op : boost::asio::coroutine
{
    error_code err_;

    fast_fail_op(error_code err) noexcept : err_(err) {}

    template <class Self>
    void operator()(Self& self)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
            self.complete(err_);
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream, BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
void boost::mysql::detail::execute_statement(
    channel<Stream>& chan,
    const statement_base& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    const execute_options&,
    resultset_base& output,
    error_code& err,
    error_info& info
)
{
    err = check_num_params(stmt, params_first, params_last, info);
    if (!err)
    {
        execute_generic(
            resultset_encoding::binary,
            chan,
            make_stmt_execute_packet(stmt.id(), params_first, params_last),
            output,
            err,
            info
        );
    }
}

template <
    class Stream,
    BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
    class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute_statement(
    channel<Stream>& chan,
    const statement_base& stmt,
    FieldViewFwdIterator params_first,
    FieldViewFwdIterator params_last,
    const execute_options&,
    resultset_base& output,
    error_info& info,
    CompletionToken&& token
)
{
    error_code err = check_num_params(stmt, params_first, params_last, info);
    if (err)
    {
        return boost::asio::async_compose<CompletionToken, void(error_code)>(
            fast_fail_op(err),
            token,
            chan
        );
    }
    return async_execute_generic(
        resultset_encoding::binary,
        chan,
        stmt_execute_request_maker_it<FieldViewFwdIterator>(stmt.id(), params_first, params_last),
        output,
        info,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple>
void boost::mysql::detail::execute_statement(
    channel<Stream>& channel,
    const statement_base& stmt,
    const FieldLikeTuple& params,
    const execute_options& opts,
    resultset_base& output,
    error_code& err,
    error_info& info
)
{
    auto params_array = tuple_to_array(params);
    execute_statement(
        channel,
        stmt,
        params_array.begin(),
        params_array.end(),
        opts,
        output,
        err,
        info
    );
}

template <class Stream, BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::async_execute_statement(
    channel<Stream>& chan,
    const statement_base& stmt,
    FieldLikeTuple&& params,
    const execute_options&,
    resultset_base& output,
    error_info& info,
    CompletionToken&& token
)
{
    using decayed_tuple = typename std::decay<FieldLikeTuple>::type;
    error_code err = check_num_params(stmt, std::tuple_size<decayed_tuple>::value, info);
    if (err)
    {
        return boost::asio::async_compose<CompletionToken, void(error_code)>(
            fast_fail_op(err),
            token,
            chan
        );
    }
    return async_execute_generic(
        resultset_encoding::binary,
        chan,
        stmt_execute_request_maker_tuple<decayed_tuple>(
            stmt.id(),
            std::forward<FieldLikeTuple>(params)
        ),
        output,
        info,
        std::forward<CompletionToken>(token)
    );
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_EXECUTE_STATEMENT_HPP_ */
