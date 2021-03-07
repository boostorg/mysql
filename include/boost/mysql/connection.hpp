//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_HPP
#define BOOST_MYSQL_CONNECTION_HPP

#include <boost/asio/ssl/context.hpp>
#include <type_traits>
#ifndef BOOST_MYSQL_DOXYGEN // For some arcane reason, Doxygen fails to expand Asio macros without this
#include "boost/mysql/detail/protocol/channel.hpp"
#include "boost/mysql/detail/protocol/protocol_types.hpp"
#include "boost/mysql/detail/network_algorithms/handshake.hpp"
#include "boost/mysql/error.hpp"
#include "boost/mysql/resultset.hpp"
#include "boost/mysql/prepared_statement.hpp"
#include "boost/mysql/connection_params.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#endif

/// The Boost libraries namespace.
namespace boost {

/// Boost.Mysql library namespace.
namespace mysql {

/**
 * \brief A connection to a MySQL server. See the following sections for how to use
 * [link mysql.queries text queries], [link mysql.prepared_statements prepared statements],
 * [link mysql.connparams connect], [link mysql.other_streams.connection handshake and quit].
 *
 * \details
 * Represents a connection to a MySQL server, allowing you to interact with it.
 * If using a socket (e.g. TCP or UNIX), consider using [reflink socket_connection].
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
protected:
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
     * \brief Initializing constructor (no user-provided SSL context).
     * \details
     * As part of the initialization, a Stream object is created
     * by forwarding any passed in arguments to its constructor.
     * If SSL ends up being used for this connection, a
     * [asioreflink ssl__context ssl::context] object will be created
     * on-demmand, with the minimum configuration settings to make SSL work.
     * In particular, no certificate validation will be performed. If you need
     * more flexibility, have a look at the other constructor overloads.
     * 
     * The constructed connection will have [refmem connection valid]
     * return `true`.
     */
    template<
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type
    >
    connection(Args&&... args) :
        channel_(new detail::channel<Stream>(nullptr, std::forward<Args>(args)...))
    {
    }

    /**
     * \brief Initializing constructor (user-provided SSL context).
     * \details
     * As part of the initialization, a Stream object is created
     * by forwarding any passed in arguments to its constructor.
     * If SSL ends up being used for this connection, `ctx` will be
     * used to initialize a [asioreflink ssl__stream ssl::stream] object.
     * By providing a SSL context you can specify extra SSL configuration options
     * like certificate verification and hostname validation. You can use a single
     * context in multiple connections.
     *
     * The provided context must be kept alive for the lifetime of the [reflink connection]
     * object. Otherwise, the results are undefined.
     * 
     * The constructed connection will have [refmem connection valid]
     * return `true`.
     */
    template<
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type
    >
    connection(boost::asio::ssl::context& ctx, Args&&... args) :
        channel_(new detail::channel<Stream>(&ctx, std::forward<Args>(args)...))
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

    /// Retrieves the underlying Stream object.
    Stream& next_layer() { return get_channel().next_layer(); }

    /// Retrieves the underlying Stream object.
    const Stream& next_layer() const { return get_channel().next_layer(); }

    /**
     * \brief Returns whether the connection uses SSL or not.
     * \details This function always returns `false` for connections that haven't been
     * established yet (handshake not run yet). If the handshake fails,
     * the return value is undefined.
     *
     * This function can be used to determine
     * whether you are using a SSL connection or not when using
     * optional SSL (see [reflink ssl_mode]).
     */
    bool uses_ssl() const noexcept { return get_channel().ssl_active(); }

    /**
     * \brief Performs the MySQL-level handshake (sync with error code version).
     * \details Does not connect the underlying stream. 
     * Prefer [refmem socket_connection connect] if possible.
     *
     * If SSL certificate validation was configured (by providing a custom SSL context
     * to this class' constructor) and fails, this function will fail.
     */
    void handshake(const connection_params& params, error_code& ec, error_info& info);

    /**
     * \brief Performs the MySQL-level handshake (sync with exceptions version).
     * \details Does not connect the underlying stream.
     * Prefer [refmem socket_connection connect] if possible.
     *
     * If SSL certificate validation was configured (by providing a custom SSL context
     * to this class' constructor) and fails, this function will fail.
     */
    void handshake(const connection_params& params);

    /**
     * \brief Performs the MySQL-level handshake
     *        (async without [reflink error_info] version).
     * \details Does not connect the underlying stream.
     * Prefer [refmem socket_connection async_connect] if possible.
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *
     * If SSL certificate validation was configured (by providing a custom SSL context
     * to this class' constructor) and fails, this function will fail.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
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
     * Prefer [refmem socket_connection async_connect] if possible.
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *
     * If SSL certificate validation was configured (by providing a custom SSL context
     * to this class' constructor) and fails, this function will fail.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
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
    resultset<Stream> query(boost::string_view query_string, error_code&, error_info&);

    /**
     * \brief Executes a SQL text query (sync with exceptions version).
     * \details See [link mysql.queries this section] for more info.
     *
     * After this function has returned, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection. Otherwise, the results are undefined.
     */
    resultset<Stream> query(boost::string_view query_string);

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
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, resultset<Stream>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
    async_query(
        boost::string_view query_string,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_query(query_string, shared_info(), std::forward<CompletionToken>(token));
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
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, resultset<Stream>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
    async_query(
        boost::string_view query_string,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Prepares a statement (sync with error code version).
     * \details See [link mysql.prepared_statements this section] for more info.
     * Prepared statements are only valid while the connection object on which
     * this function was called is alive and open.
     */
    prepared_statement<Stream> prepare_statement(boost::string_view statement, error_code&, error_info&);

    /**
     * \brief Prepares a statement (sync with exceptions version).
     * \details See [link mysql.prepared_statements this section] for more info.
     * Prepared statements are only valid while the connection object on which
     * this function was called is alive and open.
     */
    prepared_statement<Stream> prepare_statement(boost::string_view statement);

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
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, prepared_statement<Stream>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, prepared_statement<Stream>))
    async_prepare_statement(
        boost::string_view statement,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_prepare_statement(statement, shared_info(), std::forward<CompletionToken>(token));
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
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, prepared_statement<Stream>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, prepared_statement<Stream>))
    async_prepare_statement(
        boost::string_view statement,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Notifies the MySQL server that the client wants to end the session
     * (sync with error code version).
     *
     * \details Sends a quit request to the MySQL server. Both server and client should
     * close the underlying physical connection after this. This operation involves a network
     * transfer and thus can fail. This is a low-level operation.
     * See [link mysql.other_streams.connection this section] for more info.
     * Prefer [refmem socket_connection close] instead.
     */
    void quit(error_code&, error_info&);

    /**
     * \brief Notifies the MySQL server that the client wants to end the session
     * (sync with exceptions version).
     *
     * \details Sends a quit request to the MySQL server. Both server and client should
     * close the underlying physical connection after this. This operation involves a network
     * transfer and thus can fail. This is a low-level operation.
     * See [link mysql.other_streams.connection this section] for more info.
     * Prefer [refmem socket_connection close] instead.
     */
    void quit();

    /**
     * \brief Notifies the MySQL server that the client wants to end the session
     * (async without [reflink error_info] version).
     *
     * \details Sends a quit request to the MySQL server. Both server and client should
     * close the underlying physical connection after this. This operation involves a network
     * transfer and thus can fail. This is a low-level operation.
     * See [link mysql.other_streams.connection this section] for more info.
     * Prefer [refmem socket_connection async_close] instead.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
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
     * \details Sends a quit request to the MySQL server. Both server and client should
     * close the underlying physical connection after this. This operation involves a network
     * transfer and thus can fail. This is a low-level operation.
     * See [link mysql.other_streams.connection this section] for more info.
     * Prefer [refmem socket_connection async_close] instead.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
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

/**
 * \brief A connection to a MySQL server over a socket.
 * \details Extends [reflink connection] with additional
 * functions that require Stream to be a socket. Stream should
 * inherit from an instantiation of [asioreflink basic_socket basic_socket].
 *
 * In general, prefer this class over [reflink connection].
 * See also [reflink tcp_connection] and [reflink unix_connection] for
 * the most common instantiations of this template class.
 * See [link mysql.other_streams this section] for more info.
 *
 * The same considerations for copy and move operations as for
 * [reflink connection] apply.
 */
template<
class Stream
>
class socket_connection : public connection<Stream>
{
public:
    using connection<Stream>::connection;

    /// The executor type associated to this object.
    using executor_type = typename Stream::executor_type;

    /// The endpoint type associated to this connection.
    using endpoint_type = typename Stream::endpoint_type;

    /**
     * \brief Performs a connection to the MySQL server (sync with error code version).
     * \details Connects the underlying socket and then performs the handshake
     * with the server. The underlying socket is closed in case of error. Prefer
     * this function to [refmem connection handshake].
     *
     * If SSL certificate validation was configured (by providing a custom SSL context
     * to this class' constructor) and fails, this function will fail.
     */
    void connect(const endpoint_type& endpoint, const connection_params& params,
            error_code& ec, error_info& info);

    /**
     * \brief Performs a connection to the MySQL server (sync with exceptions version).
     * \details Connects the underlying socket and then performs the handshake
     * with the server. The underlying socket is closed in case of error. Prefer
     * this function to [refmem connection handshake].
     *
     * If SSL certificate validation was configured (by providing a custom SSL context
     * to this class' constructor) and fails, this function will fail.
     */
    void connect(const endpoint_type& endpoint, const connection_params& params);

    /**
     * \brief Performs a connection to the MySQL server
     *        (async without [reflink error_info] version).
     * \details
     * Connects the underlying socket and then performs the handshake
     * with the server. The underlying socket is closed in case of error. Prefer
     * this function to [refmem connection async_handshake].
     *
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *
     * If SSL certificate validation was configured (by providing a custom SSL context
     * to this class' constructor) and fails, this function will fail.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const endpoint_type& endpoint,
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
     * Connects the underlying socket and then performs the handshake
     * with the server. The underlying socket is closed in case of error. Prefer
     * this function to [refmem connection async_handshake].
     *
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *
     * If SSL certificate validation was configured (by providing a custom SSL context
     * to this class' constructor) and fails, this function will fail.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const endpoint_type& endpoint,
        const connection_params& params,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Closes the connection (sync with error code version).
     * \details Sends a quit request and closes the underlying socket.
     * Prefer this function to [refmem connection quit].
     */
    void close(error_code&, error_info&);

    /**
     * \brief Closes the connection (sync with exceptions version).
     * \details Sends a quit request and closes the underlying socket.
     * Prefer this function to [refmem connection quit].
     */
    void close();

    /**
     * \brief Closes the connection (async without [reflink error_info] version).
     * \details Sends a quit request and closes the underlying socket.
     * Prefer this function to [refmem connection async_quit].
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
        return async_close(this->shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Closes the connection (async with [reflink error_info] version).
     * \details Sends a quit request and closes the underlying socket.
     * Prefer this function to [refmem connection async_quit].
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

    /// Rebinds the connection type to another executor.
    template <class Executor>
    struct rebind_executor
    {
        /// The connection type when rebound to the specified executor.
        using other = socket_connection<
            typename Stream:: template rebind_executor<Executor>::other
        >;
    };
};

/// A connection to MySQL over a TCP socket.
using tcp_connection = socket_connection<boost::asio::ip::tcp::socket>;

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(BOOST_MYSQL_DOXYGEN)

/// A connection to MySQL over a UNIX domain socket.
using unix_connection = socket_connection<boost::asio::local::stream_protocol::socket>;

#endif

/// The default TCP port for the MySQL protocol.
constexpr unsigned short default_port = 3306;

} // mysql
} // boost

#include "boost/mysql/impl/connection.hpp"

#endif
