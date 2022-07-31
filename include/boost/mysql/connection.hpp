//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_HPP
#define BOOST_MYSQL_CONNECTION_HPP

#include <boost/asio/ssl/context.hpp>

#ifndef BOOST_MYSQL_DOXYGEN // For some arcane reason, Doxygen fails to expand Asio macros without this

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>
#include <boost/mysql/detail/network_algorithms/handshake.hpp>
#include <boost/mysql/detail/auxiliar/value_type_traits.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/prepared_statement.hpp>
#include <boost/mysql/connection_params.hpp>
#include <boost/mysql/execute_params.hpp>
#include <boost/mysql/row.hpp>
#include <type_traits>

#endif

/// The Boost libraries namespace.
namespace boost {

/// Boost.Mysql library namespace.
namespace mysql {

/**
 * \brief A connection to a MySQL server. See the following sections for how to use
 * [link mysql.queries text queries], [link mysql.prepared_statements prepared statements],
 * [link mysql.connparams connect], [link mysql.other_streams.connection handshake and quit]
 * and [link mysql.reconnecting error recovery].
 *
 * \details
 * Represents a connection to a MySQL server, allowing you to interact with it.
 *
 * Because of how the MySQL protocol works, you must fully perform an operation before
 * starting the next one. More information [link mysql.async.sequencing here].
 * Otherwise, the results are undefined.
 *
 * connection is move constructible and move assignable, but not copyable.
 * Moved-from connection objects are left in a state that makes them not
 * usable for most of the operations. The function [refmem connection valid]
 * returns whether an object is in a usable state or not. The only allowed
 * operations on moved-from connections are:
 *
 *  * Destroying them.
 *  * Participating in other move construction/assignment operations.
 *  * Calling [refmem connection valid].
 *
 * In particular, it is __not__ allowed to call [refmem connection handshake]
 * on a moved-from connection in order to re-open it.
 */
template <
    class Stream
>
class connection
{
    std::unique_ptr<detail::channel<Stream>> channel_;

    detail::channel<Stream>& get_channel() noexcept
    {
        assert(valid());
        return *channel_;
    }
    const detail::channel<Stream>& get_channel() const noexcept
    {
        assert(valid());
        return *channel_;
    }
    error_info& shared_info() noexcept { return get_channel().shared_info(); }
public:
    /**
     * \brief Initializing constructor.
     * \details
     * As part of the initialization, a Stream object is created
     * by forwarding any passed in arguments to its constructor.
     * 
     * The constructed connection will have [refmem connection valid]
     * return `true`.
     */
    template<
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type
    >
    connection(Args&&... args) :
        channel_(new detail::channel<Stream>(std::forward<Args>(args)...))
    {
    }

    /**
      * \brief Move constructor.
      * \details The constructed connection will be valid if `other` is valid.
      * After this operation, `other` is guaranteed to be invalid.
      */
    connection(connection&& other) = default;

    /**
      * \brief Move assignment.
      * \details The assigned-to connection will be valid if `other` is valid.
      */
    connection& operator=(connection&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
#endif

    /**
     * \brief Returns `true` if the object is in a valid state.
     * \details This function always returns `true` except for moved-from
     * connections. Being `valid()` is a precondition for all network
     * operations in this class.
     */
    bool valid() const noexcept { return channel_ != nullptr; }

    /// The executor type associated to this object.
    using executor_type = typename Stream::executor_type;

    /// Retrieves the executor associated to this object.
    executor_type get_executor() { return get_channel().get_executor(); }

    /// The `Stream` type this connection is using.
    using next_layer_type = Stream;

    /// Retrieves the underlying Stream object.
    Stream& next_layer() { return get_channel().next_layer(); }

    /// Retrieves the underlying Stream object.
    const Stream& next_layer() const { return get_channel().next_layer(); }

    /**
     * \brief Returns whether the connection uses SSL or not.
     * \details This function always returns `false` if the underlying
     * stream does not support SSL. This function always returns `false` 
     * for connections that haven't been
     * established yet (handshake not run yet). If the handshake fails,
     * the return value is undefined.
     *
     * This function can be used to determine
     * whether you are using a SSL connection or not when using
     * SSL negotiation (see [link mysql.ssl.negotiation this section]).
     */
    bool uses_ssl() const noexcept { return get_channel().ssl_active(); }


    /**
     * \brief Performs a connection to the MySQL server (sync with error code version).
     * \details This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *
     * Connects the underlying stream and then performs the handshake
     * with the server. The underlying stream is closed in case of error. Prefer
     * this function to [refmem connection handshake].
     *
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     */
    template <typename EndpointType>
    void connect(
        const EndpointType& endpoint,
        const connection_params& params,
        error_code& ec, 
        error_info& info
    );

    /**
     * \brief Performs a connection to the MySQL server (sync with exceptions version).
     * \details This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *
     * Connects the underlying stream and then performs the handshake
     * with the server. The underlying stream is closed in case of error. Prefer
     * this function to [refmem connection handshake].
     *
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     */
    template <typename EndpointType>
    void connect(
        const EndpointType& endpoint,
        const connection_params& params
    );

    /**
     * \brief Performs a connection to the MySQL server
     *        (async without [reflink error_info] version).
     * \details
     * This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *
     * Connects the underlying stream and then performs the handshake
     * with the server. The underlying stream is closed in case of error. Prefer
     * this function to [refmem connection async_handshake].
     *
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        typename EndpointType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const EndpointType& endpoint,
        const connection_params& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_connect(endpoint, params, this->shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Performs a connection to the MySQL server
     *        (async with [reflink error_info] version).
     * \details
     * This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *
     * Connects the underlying stream and then performs the handshake
     * with the server. The underlying stream is closed in case of error. Prefer
     * this function to [refmem connection async_handshake].
     *
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        typename EndpointType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const EndpointType& endpoint,
        const connection_params& params,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Performs the MySQL-level handshake (sync with error code version).
     * \details Does not connect the underlying stream. 
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use [refmem connection connect] instead of this function.
     *
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     */
    void handshake(const connection_params& params, error_code& ec, error_info& info);

    /**
     * \brief Performs the MySQL-level handshake (sync with exceptions version).
     * \details Does not connect the underlying stream.
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use [refmem connection connect] instead of this function.
     *
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     */
    void handshake(const connection_params& params);

    /**
     * \brief Performs the MySQL-level handshake
     *        (async without [reflink error_info] version).
     * \details Does not connect the underlying stream.
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use [refmem connection connect] instead of this function.
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_handshake(
        const connection_params& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_handshake(params, shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Performs the MySQL-level handshake
     *        (async with [reflink error_info] version).
     * \details Does not connect the underlying stream.
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use [refmem connection connect] instead of this function.
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_handshake(
        const connection_params& params,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Executes a SQL text query (sync with error code version).
     * \details See [link mysql.queries this section] for more info.
     *
     * After this function has returned, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    void query(boost::string_view query_string, resultset& result, error_code&, error_info&);

    /**
     * \brief Executes a SQL text query (sync with exceptions version).
     * \details See [link mysql.queries this section] for more info.
     *
     * After this function has returned, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    void query(boost::string_view query_string, resultset& result);

    /**
     * \brief Executes a SQL text query (async without [reflink error_info] version).
     * \details See [link mysql.queries this section] for more info.
     *
     * After the operation completes, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset<Stream>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        boost::string_view query_string,
        resultset& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_query(query_string, result, shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Executes a SQL text query (async with [reflink error_info] version).
     * \details See [link mysql.queries this section] for more info.
     *
     * After the operation completes, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset<Stream>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        boost::string_view query_string,
        resultset& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Prepares a statement (sync with error code version).
     * \details See [link mysql.prepared_statements this section] for more info.
     * Prepared statements are only valid while the connection object on which
     * this function was called is alive and open.
     */
    void prepare_statement(boost::string_view statement, prepared_statement& result, error_code&, error_info&);

    /**
     * \brief Prepares a statement (sync with exceptions version).
     * \details See [link mysql.prepared_statements this section] for more info.
     * Prepared statements are only valid while the connection object on which
     * this function was called is alive and open.
     */
    void prepare_statement(boost::string_view statement, prepared_statement& result);

    /**
     * \brief Prepares a statement (async without [reflink error_info] version).
     * \details See [link mysql.prepared_statements this section] for more info.
     * Prepared statements are only valid while the connection object on which
     * this function was called is alive and open.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::prepared_statement<Stream>)`
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_prepare_statement(
        boost::string_view statement,
        prepared_statement& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_prepare_statement(statement, result, shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Prepares a statement (async with [reflink error_info] version).
     * \details See [link mysql.prepared_statements this section] for more info.
     * Prepared statements are only valid while the connection object on which
     * this function was called is alive and open.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::prepared_statement<Stream>)`
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_prepare_statement(
        boost::string_view statement,
        prepared_statement& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );


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
    void execute_statement(
        const prepared_statement& statement,
        const ValueCollection& params,
        resultset& result,
        error_code& err,
        error_info& info
    )
    {
        return execute_statement(make_execute_params(statement, params), result, err, info);
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
    void execute_statement(
        const prepared_statement& statement,
        const ValueCollection& params,
        resultset& result
    )
    {
        return execute_statement(make_execute_params(statement, params), result);
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
    template<
        class ValueCollection,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            class EnableIf = detail::enable_if_value_collection<ValueCollection>
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute_statement(
        const prepared_statement& statement,
        const ValueCollection& params,
        resultset& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute_statement(
            make_execute_params(statement, params),
            result,
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
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
            class EnableIf = detail::enable_if_value_collection<ValueCollection>
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute_statement(
        const prepared_statement& statement,
        const ValueCollection& params,
        resultset& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute_statement(
            make_execute_params(statement, params),
            result,
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
    void execute_statement(
        const execute_params<ValueForwardIterator>& params,
        resultset& result,
        error_code& ec,
        error_info& info
    );

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
    void execute_statement(
        const execute_params<ValueForwardIterator>& params,
        resultset& result
    );

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
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute_statement(
        const execute_params<ValueForwardIterator>& params,
        resultset& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(params, result, shared_info(), std::forward<CompletionToken>(token));
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
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute_statement(
        const execute_params<ValueForwardIterator>& params,
        resultset& result,
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
    void close_statement(const prepared_statement&, error_code&, error_info&);

    /**
     * \brief Closes a prepared statement, deallocating it from the server
              (sync with exceptions version).
     * \details
     * After calling this function, no further functions may be called on this prepared
     * statement, other than assignment. Failing to do so results in undefined behavior.
     */
    void close_statement(const prepared_statement&);

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
    async_close_statement(
        const prepared_statement& stmt,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_close_statement(stmt, shared_info(), std::forward<CompletionToken>(token));
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
    async_close_statement(
        const prepared_statement& stmt,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );


    /**
     * \brief Reads a single row (sync with error code version).
     * \details Returns `true` if a row was read successfully, `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset always returns `false`.
     * 
     * If the operation succeeds and returns `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but returns `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     */
    bool read_one_row(resultset& resultset, row& output, error_code& err, error_info& info);

    /**
     * \brief Reads a single row (sync with exceptions version).
     * \details Returns `true` if a row was read successfully, `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset always returns `false`.
     * 
     * If the operation succeeds and returns `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but returns `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     */
    bool read_one_row(resultset& resultset, row& output);

    /**
     * \brief Reads a single row (async without [reflink error_info] version).
     * \details Completes with `true` if a row was read successfully, and with `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset always returns `false`.
     * 
     * If the operation succeeds and completes with `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but completes with `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, bool)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, bool))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, bool))
    async_read_one_row(
        resultset& resultset,
        row& output,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_one_row(resultset, output, shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads a single row (async with [reflink error_info] version).
     * \details Completes with `true` if a row was read successfully, and with `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset always returns `false`.
     * 
     * If the operation succeeds and completes with `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but completes with `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, bool)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, bool))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, bool))
    async_read_one_row(
        resultset& resultset,
    	row& output,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    void read_some_rows(resultset& resultset, rows_view& output, error_code& err, error_info& info);
    void read_some_rows(resultset& resultset, rows_view& output);

    /**
     * \brief Reads several rows, up to a maximum
     *        (async without [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_some_rows(
        resultset& resultset,
        rows_view& output,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_some_rows(resultset, output, shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads several rows, up to a maximum
     *        (async with [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_some_rows(
        resultset& resultset,
        rows_view& output,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /// Reads all available rows (sync with error code version).
    void read_all_rows(resultset& resultset, rows& output, error_code& err, error_info& info);

    /// Reads all available rows (sync with exceptions version).
    void read_all_rows(resultset& resultset, rows& output);

    /**
     * \brief Reads all available rows (async without [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_all_rows(
        resultset& resultset,
        rows& output,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_all_rows(resultset, output, shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads all available rows (async with [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_all_rows(
        resultset& resultset,
        rows& output,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );


    /**
     * \brief Closes the connection (sync with error code version).
     * \details 
     * This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *
     * Sends a quit request, performs the TLS shutdown (if required)
     * and closes the underlying stream. Prefer this function to [refmem connection quit].
     */
    void close(error_code&, error_info&);

    /**
     * \brief Closes the connection (sync with exceptions version).
     * \details 
     * This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *
     * Sends a quit request, performs the TLS shutdown (if required)
     * and closes the underlying stream. Prefer this function to [refmem connection quit].
     */
    void close();

    /**
     * \brief Closes the connection (async without [reflink error_info] version).
     * \details 
     * This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *
     * Sends a quit request, performs the TLS shutdown (if required)
     * and closes the underlying stream. Prefer this function to [refmem connection quit].
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_close(this->shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Closes the connection (async with [reflink error_info] version).
     * \details 
     * This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *
     * Sends a quit request, performs the TLS shutdown (if required)
     * and closes the underlying stream. Prefer this function to [refmem connection quit].
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


    /**
     * \brief Notifies the MySQL server that the client wants to end the session
     * (sync with error code version).
     *
     * \details Sends a quit request to the MySQL server. If the connection is using SSL,
     * this function will also perform the SSL shutdown. You should 
     * close the underlying physical connection after calling this function.
     *
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use [refmem connection close] instead of this function,
     * as it also takes care of closing the underlying stream.
     *
     */
    void quit(error_code&, error_info&);

    /**
     * \brief Notifies the MySQL server that the client wants to end the session
     * (sync with exceptions version).
     *
     * \details Sends a quit request to the MySQL server. If the connection is using SSL,
     * this function will also perform the SSL shutdown. You should 
     * close the underlying physical connection after calling this function.
     *
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use [refmem connection close] instead of this function,
     * as it also takes care of closing the underlying stream.
     */
    void quit();

    /**
     * \brief Notifies the MySQL server that the client wants to end the session
     * (async without [reflink error_info] version).
     *
     * \details Sends a quit request to the MySQL server. If the connection is using SSL,
     * this function will also perform the SSL shutdown. You should 
     * close the underlying physical connection after calling this function.
     *
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use [refmem connection close] instead of this function,
     * as it also takes care of closing the underlying stream.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_quit(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_quit(shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Notifies the MySQL server that the client wants to end the session
     * (async with [reflink error_info] version).
     *
     * \details Sends a quit request to the MySQL server. If the connection is using SSL,
     * this function will also perform the SSL shutdown. You should 
     * close the underlying physical connection after calling this function.
     *
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use [refmem connection close] instead of this function,
     * as it also takes care of closing the underlying stream.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_quit(
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /// Rebinds the connection type to another executor.
    template <class Executor>
    struct rebind_executor
    {
        /// The connection type when rebound to the specified executor.
        using other = connection<
            typename Stream:: template rebind_executor<Executor>::other
        >;
    };
};

/// The default TCP port for the MySQL protocol.
constexpr unsigned short default_port = 3306;

/// The default TCP port for the MySQL protocol, as a string. Useful for hostname resolution.
constexpr const char* default_port_string = "3306";

} // mysql
} // boost

#include <boost/mysql/impl/connection.hpp>

#endif
