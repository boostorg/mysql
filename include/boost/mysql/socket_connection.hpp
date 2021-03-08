//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SOCKET_CONNECTION_HPP
#define BOOST_MYSQL_SOCKET_CONNECTION_HPP

#ifndef BOOST_MYSQL_DOXYGEN // For some arcane reason, Doxygen fails to expand Asio macros without this
#include <boost/mysql/connection.hpp>
#include <boost/mysql/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#endif

namespace boost {
namespace mysql {

/**
 * \brief A connection to a MySQL server over a socket.
 * \details Extends [reflink connection] with additional
 * functions that require the undrlying stream to be a socket.
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
    class SocketStream
>
class socket_connection : public connection<SocketStream>
{
public:
    using connection<SocketStream>::connection;

    /// The executor type associated to this object.
    using executor_type = typename SocketStream::executor_type;

    /// The endpoint type associated to this connection.
    using endpoint_type = typename SocketStream::endpoint_type;

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
            typename SocketStream:: template rebind_executor<Executor>::other
        >;
    };
};

/// A connection to MySQL over a TCP socket.
using tcp_connection = socket_connection<boost::asio::ip::tcp::socket>;

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(BOOST_MYSQL_DOXYGEN)

/// A connection to MySQL over a UNIX domain socket.
using unix_connection = socket_connection<boost::asio::local::stream_protocol::socket>;

#endif

} // mysql
} // boost

#include <boost/mysql/impl/socket_connection.hpp>

#endif