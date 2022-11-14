//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_HPP
#define BOOST_MYSQL_CONNECTION_HPP

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/statement.hpp>

#include <type_traits>
#include <utility>

/// The Boost libraries namespace.
namespace boost {

/// Boost.Mysql library namespace.
namespace mysql {

/**
 * \brief A connection to a MySQL server.
 *
 * \details
 * Represents a connection to a MySQL server. You can find more info in the following sections:
 *\n
 *  * [link mysql.queries Text queries].
 *  * [link mysql.prepared_statements Prepared statements].
 *  * [link mysql.overview.async Asynchronous functions and multi-threading].
 *  * [link mysql.connparams Connect].
 *  * [link mysql.other_streams.connection Handshake and quit].
 *  * [link mysql.reconnecting Error recovery].
 *\n
 * `connection` is the main I/O object that this library implements. It owns a `Stream` object that
 * is accessed by functions involving network operations, as well as session state. You can access
 * the stream using \ref connection::stream, and its executor via \ref connection::get_executor. The
 * executor used by this object is always the same as the underlying stream. Other I/O objects
 * (`statement` and `resultset`) are proxy I/O objects, which means that they pointing to the stream
 * and state owned by `*this`.
 *\n
 * `connection` is move constructible and move assignable, but not copyable.
 * Moved-from connection objects are left in a state that makes them not
 * usable for most of the operations. The function \ref connection::valid
 * returns whether an object is in a usable state or not. The only allowed
 * operations on moved-from connections are:
 *\n
 *  * Destroying them.
 *  * Participating in other move construction/assignment operations.
 *  * Calling \ref connection::valid.
 *\n
 * In particular, it is __not__ allowed to call \ref connection::handshake
 * on a moved-from connection in order to re-open it.
 */
template <class Stream>
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
     * As part of the initialization, a `Stream` object is created
     * by forwarding any passed in arguments to its constructor.
     *
     * `this->valid()` will return `true` for the newly constructed object.
     */
    template <
        class... Args,
        class EnableIf =
            typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type>
    connection(Args&&... args)
        : channel_(new detail::channel<Stream>(0, std::forward<Args>(args)...))
    {
    }

    /**
     * \brief Move constructor.
     * \details \ref resultset and \ref statement objects referencing `other` will remain valid.
     */
    connection(connection&& other) = default;

    /**
     * \brief Move assignment.
     * \details \ref resultset and \ref statement objects referencing `other` will remain valid.
     * Objects referencing `*this` will no longer be valid. They can be re-used
     * in I/O object generting operations like \ref query or \ref prepare_statement.
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
    using stream_type = Stream;

    /// Retrieves the underlying Stream object.
    Stream& stream() { return get_channel().stream().next_layer(); }

    /// Retrieves the underlying Stream object.
    const Stream& stream() const { return get_channel().stream().next_layer(); }

    /// The type of prepared statements that can be used with this connection type.
    using statement_type = statement<Stream>;

    /// The type of resultsets that can be used with this connection type.
    using resultset_type = resultset<Stream>;

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
     * \brief Establishes a connection to a MySQL server.
     * \details This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *\n
     * Connects the underlying stream and performs the handshake
     * with the server. The underlying stream is closed in case of error. Prefer
     * this function to \ref connection::handshake.
     *\n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    template <typename EndpointType>
    void connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        error_code& ec,
        error_info& info
    );

    /// \copydoc connect
    template <typename EndpointType>
    void connect(const EndpointType& endpoint, const handshake_params& params);

    /**
     * \copydoc connect
     *\n
     * The strings pointed to by `params` should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *\n
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        typename EndpointType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_connect(
            endpoint,
            params,
            this->shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_connect
    template <
        typename EndpointType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Performs the MySQL-level handshake.
     * \details Does not connect the underlying stream.
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use \ref connection::connect instead of this function.
     *\n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * See [link mysql.ssl.handshake this section] for more info.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    void handshake(const handshake_params& params, error_code& ec, error_info& info);

    /// \copydoc handshake
    void handshake(const handshake_params& params);

    /**
     * \copydoc handshake
     *\n
     * The strings pointed to by params should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *\n
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_handshake(
        const handshake_params& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_handshake(params, shared_info(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_handshake
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_handshake(
        const handshake_params& params,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Executes a SQL text query.
     * \details Starts a multi-function operation. This function will write the query request to the
     * server and read the initial server response, but won't read the generated rows, if any. After
     * this operation completes, `result` will have \ref resultset::meta populated, and may become
     * \ref resultset::complete, if the operation did not generate any rows (e.g. it was an
     * `UPDATE`). `result` will reference `*this`, and will be usable for server interaction as long
     * as I/O object references to `*this` are valid.
     *\n
     * If the operation generated any rows, these __must__ be read (by using any of the
     * `resultset::read_xxx` functions) before engaging in any further operation involving server
     * communication. Otherwise, the results are undefined.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     *\n
     * See [link mysql.queries this section] for more info.
     */
    void query(boost::string_view query_string, resultset<Stream>& result, error_code&, error_info&);

    /// \copydoc query
    void query(boost::string_view query_string, resultset<Stream>& result);

    /**
     * \copydoc query
     * \details
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `query_string` __must be kept alive by the caller until the operation is
     * initiated__.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        boost::string_view query_string,
        resultset<Stream>& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_query(
            query_string,
            result,
            shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_query
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        boost::string_view query_string,
        resultset<Stream>& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Prepares a statement server-side.
     * \details
     * After this operation completes, `result` will reference `*this`. It will be usable for server
     * interaction as long as I/O object references to `*this` are valid.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     *\n
     * See [link mysql.prepared_statements this section] for more info.
     */
    void prepare_statement(boost::string_view stmt, statement<Stream>& result, error_code&, error_info&);

    /// \copydoc prepare_statement
    void prepare_statement(boost::string_view stmt, statement<Stream>& result);

    /**
     * \copydoc prepare_statement
     * \details
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `stmt` __must be kept alive by the caller until the operation is
     * initiated__.
     *\n
     * The handler signature for this operation is `void(boost::mysql::error_code)`
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_prepare_statement(
        boost::string_view stmt,
        statement<Stream>& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_prepare_statement(
            stmt,
            result,
            shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_prepare_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_prepare_statement(
        boost::string_view stmt,
        statement<Stream>& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Closes the connection with the server.
     * \details
     * This function is only available if `Stream` satisfies the
     * [reflink SocketStream] requirements.
     *\n
     * Sends a quit request, performs the TLS shutdown (if required)
     * and closes the underlying stream. Prefer this function to \ref connection::quit.
     *\n
     * After calling this function, any \ref statement and \ref resultset referencing `*this` will
     * no longer be usable for server interaction.
     *\n
     */
    void close(error_code&, error_info&);

    /// \copydoc close
    void close();

    /**
     * \copydoc close
     * \details
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_close(this->shared_info(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Notifies the MySQL server that the client wants to end the session and shutdowns SSL.
     * \details Sends a quit request to the MySQL server. If the connection is using SSL,
     * this function will also perform the SSL shutdown. You should
     * close the underlying physical connection after calling this function.
     *\n
     * If the `Stream` template parameter fulfills the __SocketConnection__
     * requirements, use \ref connection::close instead of this function,
     * as it also takes care of closing the underlying stream.
     */
    void quit(error_code&, error_info&);

    /// \copydoc quit
    void quit();

    /**
     * \copydoc quit
     * \details
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_quit(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_quit(shared_info(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_quit
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_quit(
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );
};

/// The default TCP port for the MySQL protocol.
constexpr unsigned short default_port = 3306;

/// The default TCP port for the MySQL protocol, as a string. Useful for hostname resolution.
constexpr const char* default_port_string = "3306";

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/connection.hpp>

#endif
