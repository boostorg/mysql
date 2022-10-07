//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_STATEMENT_HPP
#define BOOST_MYSQL_STATEMENT_HPP

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/statement_base.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/execute_params.hpp>
#include <cassert>

namespace boost {
namespace mysql {

template <class Stream>
class statement : public statement_base
{
public:
    statement() = default;
    statement(const statement&) = delete;
    statement(statement&& other) noexcept : statement_base(other) { other.reset(); }
    statement& operator=(const statement&) = delete;
    statement& operator=(statement&& rhs) noexcept { swap(rhs); return *this; }
    ~statement() = default;

    using executor_type = typename Stream::executor_type;

    executor_type get_executor() { return get_channel().get_executor(); }

    /**
     * \brief Executes a statement (collection, sync with error code version).
     * \details
     * FieldViewCollection should meet the [reflink FieldViewCollection] requirements.
     *
     * After this function has returned, you should read the entire resultset_base
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    template <class FieldViewCollection, class EnableIf = detail::enable_if_field_view_collection<FieldViewCollection>>
    void execute(
        const FieldViewCollection& params,
        resultset<Stream>& result,
        error_code& err,
        error_info& info
    )
    {
        return execute(make_execute_params(params), result, err, info);
    }

    /**
     * \brief Executes a statement (collection, sync with exceptions version).
     * \details
     * FieldViewCollection should meet the [reflink FieldViewCollection] requirements.
     *
     * After this function has returned, you should read the entire resultset_base
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    template <class FieldViewCollection, class EnableIf = detail::enable_if_field_view_collection<FieldViewCollection>>
    void execute(
        const FieldViewCollection& params,
        resultset<Stream>& result
    )
    {
        return execute_statement(make_execute_params(params), result);
    }

    /**
     * \brief Executes a statement (collection,
     *        async without [reflink error_info] version).
     * \details
     * FieldViewCollection should meet the [reflink FieldViewCollection] requirements.
     *
     * After this operation completes, you should read the entire resultset_base
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     * It is __not__ necessary to keep the collection of parameters or the
     * values they may point to alive after the initiating function returns.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset_base<Stream>)`.
     */
    template<
        class FieldViewCollection,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            class EnableIf = detail::enable_if_field_view_collection<FieldViewCollection>
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        const FieldViewCollection& params,
        resultset<Stream>& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute_statement(
            make_execute_params(params),
            result,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Executes a statement (collection,
     *        async with [reflink error_info] version).
     * \details
     * FieldViewCollection should meet the [reflink FieldViewCollection] requirements.
     *
     * After this operation completes, you should read the entire resultset_base
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     * It is __not__ necessary to keep the collection of parameters or the
     * values they may point to alive after the initiating function returns.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset_base<Stream>)`.
     */
    template <
        class FieldViewCollection,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            class EnableIf = detail::enable_if_field_view_collection<FieldViewCollection>
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        const FieldViewCollection& params,
        resultset<Stream>& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute_statement(
            make_execute_params(params),
            result,
            output_info,
            std::forward<CompletionToken>(token)
        );
    }


    /**
     * \brief Executes a statement (`execute_params`, sync with error code version).
     * \details
     * FieldViewFwdIterator should meet the [reflink FieldViewFwdIterator] requirements.
     * The range \\[`params.first()`, `params.last()`) will be used as parameters for
     * statement execution. They should be a valid iterator range.
     *
     * After this function has returned, you should read the entire resultset_base
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    template <class FieldViewFwdIterator>
    void execute(
        const execute_params<FieldViewFwdIterator>& params,
        resultset<Stream>& result,
        error_code& ec,
        error_info& info
    );

    /**
     * \brief Executes a statement (`execute_params`, sync with exceptions version).
     * \details
     * FieldViewFwdIterator should meet the [reflink FieldViewFwdIterator] requirements.
     * The range \\[`params.first()`, `params.last()`) will be used as parameters for
     * statement execution. They should be a valid iterator range.
     *
     * After this function has returned, you should read the entire resultset_base
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    template <class FieldViewFwdIterator>
    void execute(
        const execute_params<FieldViewFwdIterator>& params,
        resultset<Stream>& result
    );

    /**
     * \brief Executes a statement (`execute_params`,
     *        async without [reflink error_info] version).
     * \details
     * FieldViewFwdIterator should meet the [reflink FieldViewFwdIterator] requirements.
     * The range \\[`params.first()`, `params.last()`) will be used as parameters for
     * statement execution. They should be a valid iterator range.
     *
     * After this operation completes, you should read the entire resultset_base
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     *
     * It is __not__ necessary to keep the objects in
     * \\[`params.first()`, `params.last()`) or the
     * values they may point to alive after the initiating function returns.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset_base<Stream>)`.
     */
    template <
        class FieldViewFwdIterator,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        const execute_params<FieldViewFwdIterator>& params,
        resultset<Stream>& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(params, result, get_channel().shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Executes a statement (`execute_params`,
     *        async with [reflink error_info] version).
     * \details
     * FieldViewFwdIterator should meet the [reflink FieldViewFwdIterator] requirements.
     * The range \\[`params.first()`, `params.last()`) will be used as parameters for
     * statement execution. They should be a valid iterator range.
     *
     * After this operation completes, you should read the entire resultset_base
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     *
     * It is __not__ necessary to keep the objects in
     * \\[`params.first()`, `params.last()`) or the
     * values they may point to alive after the initiating function returns.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset_base<Stream>)`.
     */
    template <
        class FieldViewFwdIterator,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        const execute_params<FieldViewFwdIterator>& params,
        resultset<Stream>& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );


    /**
     * \brief Closes a prepared statement, deallocating it from the server
              (sync with error code version).
     * \details
     * After calling this function, no further functions may be called on this prepared
     * statement, other than assignment. Failing to do so results in undefined behavior.
     */
    void close(error_code&, error_info&);

    /**
     * \brief Closes a prepared statement, deallocating it from the server
              (sync with exceptions version).
     * \details
     * After calling this function, no further functions may be called on this prepared
     * statement, other than assignment. Failing to do so results in undefined behavior.
     */
    void close();

    /**
     * \brief Closes a prepared statement, deallocating it from the server
              (async without [reflink error_info] version).
     * \details
     * After the operation completes, no further functions may be called on this prepared
     * statement, other than assignment. Failing to do so results in undefined behavior.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_close(get_channel().shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Closes a prepared statement, deallocating it from the server
              (async with [reflink error_info] version).
     * \details
     * After the operation completes, no further functions may be called on this prepared
     * statement, other than assignment. Failing to do so results in undefined behavior.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

private:
    detail::channel<Stream>& get_channel() noexcept
    {
        assert(valid());
        return *static_cast<detail::channel<Stream>*>(channel_ptr());
    }
};

} // mysql
} // boost

#include <boost/mysql/impl/statement.hpp>

#endif
