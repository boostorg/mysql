//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PREPARED_STATEMENT_HPP
#define BOOST_MYSQL_PREPARED_STATEMENT_HPP

#include "boost/mysql/resultset.hpp"
#include "boost/mysql/execute_params.hpp"
#include "boost/mysql/detail/protocol/channel.hpp"
#include "boost/mysql/detail/protocol/prepared_statement_messages.hpp"
#include "boost/mysql/detail/auxiliar/value_type_traits.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <type_traits>

namespace boost {
namespace mysql {

/// Convenience constant to use when executing a statement without parameters.
constexpr std::array<value, 0> no_statement_params {};

/**
 * \brief Represents a prepared statement. See [link mysql.prepared_statements] for more info.
 * \details This class is a handle to a server-side prepared statement.
 *
 * The main use of a prepared statement is executing it
 * using [refmem prepared_statement execute], which yields a [reflink resultset].
 *
 * Prepared statements are default-constructible and movable, but not copyable. 
 * [refmem prepared_statement valid] returns `false` for default-constructed 
 * and moved-from prepared statements. Calling any member function on an invalid
 * prepared statements, other than assignment, results in undefined behavior.
 *
 * Prepared statements are managed by the server in a per-connection basis:
 * once created, a prepared statement may be used as long as the parent
 * [reflink connection] object (i.e. the connection that originated the resultset)
 * is alive and open. Calling any function on a prepared_statement
 * whose parent connection has been closed or destroyed results
 * in undefined behavior.
 */
template <class Stream>
class prepared_statement
{
    detail::channel_observer_ptr<Stream> channel_;
    detail::com_stmt_prepare_ok_packet stmt_msg_;

    template <class ValueForwardIterator>
    void check_num_params(ValueForwardIterator first, ValueForwardIterator last, error_code& err, error_info& info) const;

    error_info& shared_info() noexcept { assert(channel_); return channel_->shared_info(); }

    struct async_execute_initiation;
public:
    /**
      * \brief Default constructor. 
      * \details Default constructed statements have [refmem prepared_statement valid] return `false`.
      */
    prepared_statement() = default;

    /**
      * \brief Move constructor.
      * \details The constructed statement will be valid if `other` is valid.
      * After this operation, `other` is guaranteed to be invalid.
      */
    prepared_statement(prepared_statement&& other) = default;

    /**
      * \brief Move assignment.
      * \details The assigned-to statement will be valid if `other` is valid.
      */
    prepared_statement& operator=(prepared_statement&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    prepared_statement(const prepared_statement&) = delete;
    prepared_statement& operator=(const prepared_statement&) = delete;

    // Private. Do not use.
    prepared_statement(detail::channel<Stream>& chan, const detail::com_stmt_prepare_ok_packet& msg) noexcept:
        channel_(&chan), stmt_msg_(msg) {}
#endif

    /// The executor type associated to this object.
    using executor_type = typename Stream::executor_type;

    /// Retrieves the executor associated to this object.
    executor_type get_executor() { assert(channel_);  return channel_->get_executor(); }

    /// Retrieves the stream object associated with the underlying connection.
    Stream& next_layer() noexcept { assert(channel_); return channel_->next_layer(); }

    /// Retrieves the stream object associated with the underlying connection.
    const Stream& next_layer() const noexcept { assert(channel_); return channel_->next_layer(); }

    /**
     * \brief Returns `true` if the statement is not a default-constructed object.
     * \details Calling any function other than assignment on an statement for which
     * this function returns `false` results in undefined behavior.
     */
    bool valid() const noexcept { return channel_ != nullptr; }

    /// Returns a server-side identifier for the statement (unique in a per-connection basis).
    unsigned id() const noexcept { assert(valid()); return stmt_msg_.statement_id; }

    /// Returns the number of parameters that should be provided when executing the statement.
    unsigned num_params() const noexcept { assert(valid()); return stmt_msg_.num_params; }

    /**
     * \brief Executes a statement (collection, sync with error code version).
     * \details
     * ValueCollection should meet the [reflink ValueCollection] requirements.
     *
     * After this function has returned, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    template <class ValueCollection, class EnableIf = detail::enable_if_value_collection<ValueCollection>>
    resultset<Stream> execute(const ValueCollection& params, error_code& err, error_info& info)
    {
        return execute(make_execute_params(params), err, info);
    }

    /**
     * \brief Executes a statement (collection, sync with exceptions version).
     * \details
     * ValueCollection should meet the [reflink ValueCollection] requirements.
     *
     * After this function has returned, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    template <class ValueCollection, class EnableIf = detail::enable_if_value_collection<ValueCollection>>
    resultset<Stream> execute(const ValueCollection& params)
    {
        return execute(make_execute_params(params));
    }

    /**
     * \brief Executes a statement (collection,
     *        async without [reflink error_info] version).
     * \details
     * ValueCollection should meet the [reflink ValueCollection] requirements.
     *
     * After this operation completes, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     * It is __not__ necessary to keep the collection of parameters or the
     * values they may point to alive after the initiating function returns.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset<Stream>)`.
     */
    template <
        class ValueCollection,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, resultset<Stream>))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            class EnableIf = detail::enable_if_value_collection<ValueCollection>
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
    async_execute(
        const ValueCollection& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(
            make_execute_params(params),
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Executes a statement (collection,
     *        async with [reflink error_info] version).
     * \details
     * ValueCollection should meet the [reflink ValueCollection] requirements.
     *
     * After this operation completes, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     * It is __not__ necessary to keep the collection of parameters or the
     * values they may point to alive after the initiating function returns.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset<Stream>)`.
     */
    template <
        class ValueCollection,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, resultset<Stream>))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            class EnableIf = detail::enable_if_value_collection<ValueCollection>
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
    async_execute(
        const ValueCollection& params,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(
            make_execute_params(params),
            output_info,
            std::forward<CompletionToken>(token)
        );
    }


    /**
     * \brief Executes a statement (`execute_params`, sync with error code version).
     * \details
     * ValueForwardIterator should meet the [reflink ValueForwardIterator] requirements.
     * The range \\[`params.first()`, `params.last()`) will be used as parameters for
     * statement execution. They should be a valid iterator range.
     *
     * After this function has returned, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    template <class ValueForwardIterator>
    resultset<Stream> execute(const execute_params<ValueForwardIterator>& params,
            error_code&, error_info&);

    /**
     * \brief Executes a statement (`execute_params`, sync with exceptions version).
     * \details
     * ValueForwardIterator should meet the [reflink ValueForwardIterator] requirements.
     * The range \\[`params.first()`, `params.last()`) will be used as parameters for
     * statement execution. They should be a valid iterator range.
     *
     * After this function has returned, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    template <class ValueForwardIterator>
    resultset<Stream> execute(const execute_params<ValueForwardIterator>& params);

    /**
     * \brief Executes a statement (`execute_params`,
     *        async without [reflink error_info] version).
     * \details
     * ValueForwardIterator should meet the [reflink ValueForwardIterator] requirements.
     * The range \\[`params.first()`, `params.last()`) will be used as parameters for
     * statement execution. They should be a valid iterator range.
     *
     * After this operation completes, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     *
     * It is __not__ necessary to keep the objects in
     * \\[`params.first()`, `params.last()`) or the
     * values they may point to alive after the initiating function returns.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset<Stream>)`.
     */
    template <
        class ValueForwardIterator,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, resultset<Stream>))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
    async_execute(
        const execute_params<ValueForwardIterator>& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(params, shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Executes a statement (`execute_params`,
     *        async with [reflink error_info] version).
     * \details
     * ValueForwardIterator should meet the [reflink ValueForwardIterator] requirements.
     * The range \\[`params.first()`, `params.last()`) will be used as parameters for
     * statement execution. They should be a valid iterator range.
     *
     * After this operation completes, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     *
     * It is __not__ necessary to keep the objects in
     * \\[`params.first()`, `params.last()`) or the
     * values they may point to alive after the initiating function returns.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset<Stream>)`.
     */
    template <
        class ValueForwardIterator,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, resultset<Stream>))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
    async_execute(
        const execute_params<ValueForwardIterator>& params,
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
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_close(shared_info(), std::forward<CompletionToken>(token));
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
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /// Rebinds the prepared statement type to another executor.
    template <class Executor>
    struct rebind_executor
    {
        /// The prepared statement type when rebound to the specified executor.
        using other = prepared_statement<
            typename Stream:: template rebind_executor<Executor>::other
        >;
    };
};

/// A prepared statement associated to a [reflink tcp_connection].
using tcp_prepared_statement = prepared_statement<boost::asio::ip::tcp::socket>;

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(BOOST_MYSQL_DOXYGEN)

/// A prepared statement associated to a [reflink unix_connection].
using unix_prepared_statement = prepared_statement<boost::asio::local::stream_protocol::socket>;

#endif

} // mysql
} // boost

#include "boost/mysql/impl/prepared_statement.hpp"

#endif /* INCLUDE_BOOST_MYSQL_PREPARED_STATEMENT_HPP_ */
