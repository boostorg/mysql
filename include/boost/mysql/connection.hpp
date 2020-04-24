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
#include "boost/mysql/detail/auxiliar/async_result_macro.hpp"
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
 * Because of how the MySQL protocol works, you must fully perform an operation before
 * starting the next one. For queries, you must wait for the query response and
 * **read the entire resultset** before starting a new query. For prepared statements,
 * once executed, you must wait for the response and read the entire resultset.
 * Otherwise, results are undefined.
 */
template <
    typename Stream ///< The underlying stream to use; must satisfy Boost.Asio's SyncStream and AsyncStream concepts.
>
class connection
{
    using channel_type = detail::channel<Stream>;

    Stream next_layer_;
    channel_type channel_;
public:
    /**
     * \brief Initializing constructor.
     * \details Creates a Stream object by forwarding any passed in arguments to its constructor.
     */
    template <typename... Args>
    connection(Args&&... args) :
        next_layer_(std::forward<Args>(args)...),
        channel_(next_layer_)
    {
    }

    /// Retrieves the underlying Stream object.
    Stream& next_layer() { return next_layer_; }

    /// Retrieves the underlying Stream object.
    const Stream& next_layer() const { return next_layer_; }

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
    bool uses_ssl() const noexcept { return channel_.ssl_active(); }

    /// Performs the MySQL-level handshake (synchronous with error code version).
    void handshake(const connection_params& params, error_code& ec, error_info& info);

    /// Performs the MySQL-level handshake (synchronous with exceptions version).
    void handshake(const connection_params& params);

    /// Handler signature for connection::async_handshake.
    using handshake_signature = void(error_code);

    /**
     * \brief Performs the MySQL-level handshake (asynchronous version).
     * \details The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     */
    template <typename CompletionToken>
    BOOST_MYSQL_INITFN_RESULT_TYPE(CompletionToken, handshake_signature)
    async_handshake(const connection_params& params, CompletionToken&& token, error_info* info = nullptr);

    template <typename EndpointType>
    void connect(const EndpointType& endpoint, const connection_params& params,
            error_code& ec, error_info& info);

    template <typename EndpointType>
    void connect(const EndpointType& endpoint, const connection_params& params);

    using connect_signature = void(error_code);

    template <typename EndpointType, typename CompletionToken>
    BOOST_MYSQL_INITFN_RESULT_TYPE(CompletionToken, connect_signature)
    async_connect(const EndpointType& endpoint, const connection_params& params,
            CompletionToken&& token, error_info* output_info=nullptr);

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

    /// Handler signature for connection::async_query.
    using query_signature = void(error_code, resultset<Stream>);

    /// Executes a SQL text query (async version).
    template <typename CompletionToken>
    BOOST_MYSQL_INITFN_RESULT_TYPE(CompletionToken, query_signature)
    async_query(std::string_view query_string, CompletionToken&& token, error_info* info=nullptr);

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

    /// Handler signature for connection::async_prepare_statement.
    using prepare_statement_signature = void(error_code, prepared_statement<Stream>);

    /// Prepares a statement (async version).
    template <typename CompletionToken>
    BOOST_MYSQL_INITFN_RESULT_TYPE(CompletionToken, prepare_statement_signature)
    async_prepare_statement(std::string_view statement, CompletionToken&& token, error_info* info=nullptr);

    /**
     * \brief Notifies the MySQL server that we want to end the session and quit the connection
     * (sync with error code version).
     *
     * \details Sends a quit request to the MySQL server. Both server and client should
     * close the underlying physical connection after this. This operation involves a network
     * transfer and thus can fail.
     *
     * \warning This operation is a low level one. It does not close the underlying
     * physical connection after sending the quit request. Unless there is no other
     * option, prefer connection::close instead.
     */
    void quit(error_code&, error_info&);

    /**
     * \brief Notifies the MySQL server that we want to end the session and quit the connection
     * (sync with exceptions version).
     */
    void quit();

    /// Handler signature for connection::async_quit.
    using quit_signature = void(error_code);

    /**
     * \brief Notifies the MySQL server that we want to end the session and quit the connection
     * (async version).
     */
    template <typename CompletionToken>
    BOOST_MYSQL_INITFN_RESULT_TYPE(CompletionToken, quit_signature)
    async_quit(CompletionToken&& token, error_info* info=nullptr);

    /**
     * \brief Closes the connection (sync with error code version).
     *
     * \details Performs a full shutdown of the connection to the MySQL server.
     * This implies sending a quit request and then closing the underlying
     * physical connection. This operation is thus roughly equivalent to
     * a connection::quit plus a close on the underlying socket. It should
     * be preferred over connection::quit whenever possible.
     *
     * \warning This function is only available if Stream is a socket
     * (a derived class on an instantiation of boost::asio::basic_socket).
     * It is applyable in the most common cases (boost::mysql::tcp_connection and
     * boost::mysql::unix_connection).
     */
    void close(error_code&, error_info&);

    /// Closes the connection (sync with exceptions version).
    void close();

    /// Handler signature for connection::async_close.
    using close_signature = void(error_code);

    /// Closes the connection (async version).
    template <typename CompletionToken>
    BOOST_MYSQL_INITFN_RESULT_TYPE(CompletionToken, close_signature)
    async_close(CompletionToken&& token, error_info* info=nullptr);
};

/**
 * \ingroup connection
 * \brief A connection to MySQL over a TCP socket.
 */
using tcp_connection = connection<boost::asio::ip::tcp::socket>;

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(BOOST_MYSQL_DOXYGEN)

/**
 * \ingroup connection
 * \brief A connection to MySQL over a UNIX domain socket.
 */
using unix_connection = connection<boost::asio::local::stream_protocol::socket>;

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
