//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP

#include "boost/mysql/detail/network_algorithms/execute_statement.hpp"
#include "boost/mysql/detail/network_algorithms/close_statement.hpp"
#include "boost/mysql/detail/auxiliar/stringize.hpp"
#include <boost/asio/bind_executor.hpp>


template <class Stream>
template <class ValueForwardIterator>
void boost::mysql::prepared_statement<Stream>::check_num_params(
    ValueForwardIterator first,
    ValueForwardIterator last,
    error_code& err,
    error_info& info
) const
{
    auto param_count = std::distance(first, last);
    if (param_count != num_params())
    {
        err = detail::make_error_code(errc::wrong_num_params);
        info.set_message(detail::stringize(
                "prepared_statement::execute: expected ", num_params(), " params, but got ", param_count));
    }
}


template <class Stream>
template <class ValueForwardIterator>
boost::mysql::resultset<Stream> boost::mysql::prepared_statement<Stream>::execute(
    const execute_params<ValueForwardIterator>& params,
    error_code& err,
    error_info& info
)
{
    assert(valid());

    mysql::resultset<Stream> res;
    detail::clear_errors(err, info);

    // Verify we got passed the right number of params
    check_num_params(params.first(), params.last(), err, info);
    if (!err)
    {
        detail::execute_statement(
            *channel_,
            stmt_msg_.statement_id,
            params.first(),
            params.last(),
            res,
            err,
            info
        );
    }

    return res;
}

template <class Stream>
template <class ValueForwardIterator>
boost::mysql::resultset<Stream> boost::mysql::prepared_statement<Stream>::execute(
    const execute_params<ValueForwardIterator>& params
)
{
    detail::error_block blk;
    auto res = execute(params, blk.err, blk.info);
    blk.check();
    return res;
}

// Helper for async_execute
template <class Stream>
struct boost::mysql::prepared_statement<Stream>::async_execute_initiation
{
    template <class HandlerType>
    struct error_handler
    {
        error_code err;
        HandlerType h;

        void operator()()
        {
            std::forward<HandlerType>(h)(err, resultset<Stream>());
        }
    };

    template <class HandlerType, class ValueForwardIterator>
    void operator()(
        HandlerType&& handler,
        error_code err,
        error_info& info,
        prepared_statement<Stream>& stmt,
        ValueForwardIterator params_first,
        ValueForwardIterator params_last
    ) const
    {
        if (err)
        {
            auto executor = boost::asio::get_associated_executor(
                handler,
                stmt.next_layer().get_executor()
            );

            boost::asio::post(boost::asio::bind_executor(
                executor,
                error_handler<HandlerType>{err, std::forward<HandlerType>(handler)}
            ));
        }
        else
        {
            // Actually execute the statement
            detail::async_execute_statement(
                *stmt.channel_,
                stmt.stmt_msg_.statement_id,
                params_first,
                params_last,
                std::forward<HandlerType>(handler),
                info
            );
        }
    }
};

template <class Stream>
template <class ValueForwardIterator, BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(boost::mysql::error_code, boost::mysql::resultset<Stream>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, boost::mysql::resultset<Stream>)
)
boost::mysql::prepared_statement<Stream>::async_execute(
    const execute_params<ValueForwardIterator>& params,
    error_info& output_info,
    CompletionToken&& token
)
{
    output_info.clear();
    assert(valid());

    // Check we got passed the right number of params
    error_code err;
    check_num_params(params.first(), params.last(), err, output_info);

    return boost::asio::async_initiate<CompletionToken, void(error_code, resultset<Stream>)>(
        async_execute_initiation(),
        token,
        err,
        std::ref(output_info),
        std::ref(*this),
        params.first(),
        params.last()
    );
}

template <class Stream>
void boost::mysql::prepared_statement<Stream>::close(
    error_code& code,
    error_info& info
)
{
    assert(valid());
    detail::clear_errors(code, info);
    detail::close_statement(*channel_, id(), code, info);
}

template <class Stream>
void boost::mysql::prepared_statement<Stream>::close()
{
    assert(valid());
    detail::error_block blk;
    detail::close_statement(*channel_, id(), blk.err, blk.info);
    blk.check();
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::prepared_statement<Stream>::async_close(
    error_info& output_info,
    CompletionToken&& token
)
{
    assert(valid());
    output_info.clear();
    return detail::async_close_statement(
        *channel_,
        id(),
        std::forward<CompletionToken>(token),
        output_info
    );
}

#endif /* INCLUDE_BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP_ */
