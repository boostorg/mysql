//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_HPP
#define BOOST_MYSQL_CONNECTION_HPP

#include "boost/mysql/detail/protocol/channel.hpp"
#include "boost/mysql/detail/protocol/protocol_types.hpp"
#include "boost/mysql/detail/network_algorithms/handshake.hpp"
#include "boost/mysql/error.hpp"
#include "boost/mysql/resultset.hpp"
#include "boost/mysql/prepared_statement.hpp"
#include "boost/mysql/connection_params.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>

/**
 * \defgroup connection Connection
 * \brief Classes and functions related to establishing
 * connections with the MySQL server.
 */

/**
 * \namespace boost The Boost C++ libraries namespace.
 * \namespace boost::mysql The MySQL Boost library namespace.
 */

namespace boost {
namespace mysql {

/**
 * \ingroup connection
 * \brief A connection to a MySQL server.
 * \details
 * This is the basic object to instantiate to use the MySQL-Asio library.
 *
 * Connections allow you to interact with the database server (e.g. you
 * can issue queries using connection::query).
 *
 * Before being able to use a connection, you must connect it. This encompasses two steps:
 *   - Stream connection: you should make sure the Stream gets connected to
 *     whatever endpoint the MySQL server lives on. You can access the underlying
 *     Stream object by calling connection::next_layer(). For a TCP socket, this is equivalent
 *     to connecting the socket.
 *   - MySQL handshake: authenticates the connection to the MySQL server. You can do that
 *     by calling connection::handshake() or connection::async_handshake().
 *
 * If using a TCP socket or UNIX socket, consider using socket_connection,
 * which implements a socket_connection::connect method that handles both steps.
 *
 * Because of how the MySQL protocol works, you must fully perform an operation before
 * starting the next one. For queries, you must wait for the query response and
 * **read the entire resultset** before starting a new query. For prepared statements,
 * once executed, you must wait for the response and read the entire resultset.
 * Otherwise, results are undefined.
 *
 * connection is move constructible and move assignable, but not copyable.
 * Moved-from connection objects are left in a state that makes them not
 * usable for most of the operations. The function connection::valid()
 * returns whether an object is in a usable state or not. The only allowed
 * operations on moved-from connections are:
 *   - Destroying them.
 *   - Participating in other move construction/assignment operations.
 *   - Calling the connection::valid() function.
 *
 * In particular, it is **not** allowed to call connection::handshake
 * on a moved-from connection in order to re-open it.
 */
template <
    typename Stream ///< The underlying stream to use; must satisfy Boost.Asio's SyncStream and AsyncStream concepts.
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
     * \brief Initializing constructor.
     * \details Creates a Stream object by forwarding any passed in arguments to its constructor.
     */
    template <typename... Args>
    connection(Args&&... args) :
        channel_(new detail::channel<Stream>(std::forward<Args>(args)...))
    {
    }

    /**
     * \brief Returns true if the object is in a valid state.
     * \details This function always returns true except for moved-from
     * connections. Being valid() is a precondition for all network
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
     * \details Will always return false for connections that haven't been
     * established yet (handshake not run yet). If the handshake fails,
     * the return value is undefined.
     *
     * This function can be used to determine
     * whether you are using a SSL connection or not when using
     * optional SSL (ssl_mode::enable).
     */
    bool uses_ssl() const noexcept { return get_channel().ssl_active(); }

    /// Performs the MySQL-level handshake (synchronous with error code version).
    void handshake(const connection_params& params, error_code& ec, error_info& info);

    /// Performs the MySQL-level handshake (synchronous with exceptions version).
    void handshake(const connection_params& params);

    /**
     * \brief Performs the MySQL-level handshake (asynchronous version without error_info).
     * \details The handler signature for this operation is
     * `void(boost::mysql::error_code)`.
     *
     * \warning The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
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

    /// Performs the MySQL-level handshake (asynchronous version with error_info).
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
     * \details Does not perform the actual retrieval of the data; use the various
     * fetch functions within resultset to achieve that.
     * \see resultset
     *
     * Note that query_string may contain any valid SQL, not just SELECT statements.
     * If your query does not return any data, then the resultset will be empty.
     *
     * \warning After query() has returned, you should read the entire resultset
     * before calling any function that involves communication with the server over this
     * connection (like connection::query, connection::prepare_statement or
     * prepared_statement::execute). Otherwise, the results are undefined.
     */
    resultset<Stream> query(std::string_view query_string, error_code&, error_info&);

    /// Executes a SQL text query (sync with exceptions version).
    resultset<Stream> query(std::string_view query_string);

    /**
     * \brief Executes a SQL text query (async version without error_info).
     * \details The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::resultset<Stream>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, resultset<Stream>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
    async_query(
        std::string_view query_string,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_query(query_string, shared_info(), std::forward<CompletionToken>(token));
    }

    /// Executes a SQL text query (async version with error_info).
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, resultset<Stream>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, resultset<Stream>))
    async_query(
        std::string_view query_string,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Prepares a statement in the server (sync with error code version).
     * \details Instructs the server to create a prepared statement. The passed
     * in statement should be a SQL statement with question marks (?) as placeholders
     * for the statement parameters. See the MySQL documentation on prepared statements
     * for more info.
     *
     * Prepared statements are only valid while the connection object on which
     * prepare_statement was called is alive and open. See prepared_statement docs
     * for more info.
     */
    prepared_statement<Stream> prepare_statement(std::string_view statement, error_code&, error_info&);

    /// Prepares a statement (sync with exceptions version).
    prepared_statement<Stream> prepare_statement(std::string_view statement);

    /**
     * \brief Prepares a statement (async version without error_info).
     * \details The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::prepared_statement<Stream>)`
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, prepared_statement<Stream>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, prepared_statement<Stream>))
    async_prepare_statement(
        std::string_view statement,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_prepare_statement(statement, shared_info(), std::forward<CompletionToken>(token));
    }

    /// Prepares a statement (async version with error_info).
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, prepared_statement<Stream>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, prepared_statement<Stream>))
    async_prepare_statement(
        std::string_view statement,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Notifies the MySQL server that we want to end the session and quit the connection
     * (sync with error code version).
     *
     * \details Sends a quit request to the MySQL server. Both server and client should
     * close the underlying physical connection after this. This operation involves a network
     * transfer and thus can fail.
     *
     * \warning This operation is a low level one. It does not close the underlying
     * physical connection after sending the quit request. If you are using a
     * socket_stream, use socket_stream::close instead.
     */
    void quit(error_code&, error_info&);

    /**
     * \brief Notifies the MySQL server that we want to end the session and quit the connection
     * (sync with exceptions version).
     */
    void quit();

    /**
     * \brief Notifies the MySQL server that we want to end the session and quit the connection
     * (async version without error_info).
     * \details The handler signature for this operation is
     * `void(boost::mysql::error_code)`.
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

    /// Notifies the MySQL server that we want to end the session and quit the connection
    /// (async version with error_info).
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
    template <typename Executor>
    struct rebind_executor
    {
        /// The connection type when rebound to the specified executor.
        using other = connection<
            typename Stream:: template rebind_executor<Executor>::other
        >;
    };
};

/**
 * \ingroup connection
 * \brief A connection to a MySQL server over a socket.
 * \details Extends boost::mysql::connection with additional
 * functions that require Stream to be a socket. Stream should
 * inherit from an instantiation of boost::asio::basic_socket.
 *
 * In general, prefer this class over boost::mysql::connection.
 * See also boost::mysql::tcp_socket and boost::mysql::unix_socket for
 * the most common instantiations of this template class.
 *
 * When using this class, use socket_connection::connect instead of
 * a socket connect + connection::handshake, and socket_connection::close
 * instead of a connection::quit + socket close.
 *
 * The same considerations for copy and move operations as for
 * boost::mysql::connection apply.
 */
template<
    typename Stream
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
     * this function to connection::handshake if using this class.
     */
    void connect(const endpoint_type& endpoint, const connection_params& params,
            error_code& ec, error_info& info);

    /// Performs a connection to the MySQL server (sync with exceptions version).
    void connect(const endpoint_type& endpoint, const connection_params& params);

    /**
     * \brief Performs a connection to the MySQL server (async version without error_info).
     * \details The handler signature for this operation is
     * `void(boost::mysql::error_code)`.
     *
     * \warning The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
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

    /// Performs a connection to the MySQL server (async version with error_info).
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
     *
     * \details Performs a full shutdown of the connection to the MySQL server.
     * This implies sending a quit request and then closing the underlying
     * physical connection. This operation is thus roughly equivalent to
     * a connection::quit plus a close on the underlying socket. It should
     * be preferred over connection::quit whenever possible.
     */
    void close(error_code&, error_info&);

    /// Closes the connection (sync with exceptions version).
    void close();

    /**
     * \brief Closes the connection (async version without error_info).
     * \details The handler signature for this operation is
     * `void(boost::mysql::error_code)`.
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

    /// Closes the connection (async version with error_info).
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
    template <typename Executor>
    struct rebind_executor
    {
        /// The connection type when rebound to the specified executor.
        using other = socket_connection<
            typename Stream:: template rebind_executor<Executor>::other
        >;
    };
};

/**
 * \ingroup connection
 * \brief A connection to MySQL over a TCP socket.
 */
using tcp_connection = socket_connection<boost::asio::ip::tcp::socket>;

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(BOOST_MYSQL_DOXYGEN)

/**
 * \ingroup connection
 * \brief A connection to MySQL over a UNIX domain socket.
 */
using unix_connection = socket_connection<boost::asio::local::stream_protocol::socket>;

#endif

/**
 * \ingroup connection
 * \brief The default TCP port for the MySQL protocol.
 */
constexpr unsigned short default_port = 3306;

} // mysql
} // boost

#include "boost/mysql/impl/connection.hpp"

#endif
