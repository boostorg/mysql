//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_HPP
#define BOOST_MYSQL_CONNECTION_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/server_diagnostics.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/rebind_executor.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/protocol/protocol_types.hpp>

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
 * Represents a connection to a MySQL server.
 *\n
 * `connection` is the main I/O object that this library implements. It owns a `Stream` object that
 * is accessed by functions involving network operations, as well as session state. You can access
 * the stream using \ref connection::stream, and its executor via \ref connection::get_executor. The
 * executor used by this object is always the same as the underlying stream.
 */
template <class Stream>
class connection
{
    std::unique_ptr<detail::channel<Stream>> channel_;

    const detail::channel<Stream>& get_channel() const noexcept
    {
        assert(channel_ != nullptr);
        return *channel_;
    }
    server_diagnostics& shared_diag() noexcept { return get_channel().shared_diag(); }

public:
    // TODO: hide this
    detail::channel<Stream>& get_channel() noexcept
    {
        assert(channel_ != nullptr);
        return *channel_;
    }

    /**
     * \brief Initializing constructor.
     * \details
     * As part of the initialization, a `Stream` object is created
     * by forwarding any passed in arguments to its constructor.
     */
    template <
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type>
    connection(Args&&... args) : channel_(new detail::channel<Stream>(0, std::forward<Args>(args)...))
    {
    }

    /**
     * \brief Move constructor.
     * \details \ref statement objects referencing `other` remain usable for I/O operations.
     */
    connection(connection&& other) = default;

    /**
     * \brief Move assignment.
     * \details \ref statement objects referencing `other` remain usable for I/O operations.
     * Statements referencing `*this` will no longer be usable.
     */
    connection& operator=(connection&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
#endif

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

    /**
     * \brief Returns whether the connection uses SSL or not.
     * \details This function always returns `false` if the underlying
     * stream does not support SSL. This function always returns `false`
     * for connections that haven't been
     * established yet (handshake not run yet). If the handshake fails,
     * the return value is undefined.
     *\n
     * This function can be used to determine whether you are using a SSL
     * connection or not when using SSL negotiation.
     */
    bool uses_ssl() const noexcept { return get_channel().ssl_active(); }

    /**
     * \brief Establishes a connection to a MySQL server.
     * \details This function is only available if `Stream` satisfies the
     * `SocketStream` concept.
     *\n
     * Connects the underlying stream and performs the handshake
     * with the server. The underlying stream is closed in case of error. Prefer
     * this function to \ref connection::handshake.
     *\n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    template <typename EndpointType>
    void connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        error_code& ec,
        server_diagnostics& diagnostics
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
        return async_connect(endpoint, params, this->shared_diag(), std::forward<CompletionToken>(token));
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
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Performs the MySQL-level handshake.
     * \details Does not connect the underlying stream.
     * If the `Stream` template parameter fulfills the `SocketConnection`
     * requirements, use \ref connection::connect instead of this function.
     *\n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    void handshake(const handshake_params& params, error_code& ec, server_diagnostics& diag);

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
        return async_handshake(params, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_handshake
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_handshake(
        const handshake_params& params,
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Executes a SQL text query.
     * \details
     * Sends `query_string` to the server for execution and reads the response into `result`.
     * `query_string` should be encoded using the connection's character set.
     *\n
     * After this operation completes successfully, `result.has_value() == true`.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    void query(string_view query_string, resultset& result, error_code&, server_diagnostics&);

    /// \copydoc query
    void query(string_view query_string, resultset& result);

    /**
     * \copydoc query
     * \details
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `query_string` __must be kept alive by the caller until the operation is
     * initiated__.
     *\n
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        string_view query_string,
        resultset& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_query(query_string, result, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_query
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        string_view query_string,
        resultset& result,
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Starts a text query as a multi-function operation.
     * \details Writes the query request and reads the initial server response and the column
     * metadata, but not the generated rows, if any. After this operation completes, `st` will have
     * \ref execution_state::meta populated, and may become \ref execution_state::complete
     * if the operation did not generate any rows (e.g. it was an `UPDATE`).
     *\n
     * If the operation generated any rows, these <b>must</b> be read (by using \ref read_one_row or
     * \ref read_some_rows) before engaging in any further operation involving network reads.
     * Otherwise, the results are undefined.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     *\n
     * `query_string` should be encoded using the connection's character set.
     */
    void start_query(string_view query_string, execution_state& st, error_code&, server_diagnostics&);

    /// \copydoc start_query
    void start_query(string_view query_string, execution_state& st);

    /**
     * \copydoc start_query
     * \details
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `query_string` <b>must be kept alive by the caller until the operation is
     * initiated</b>.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_query(
        string_view query_string,
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_start_query(query_string, st, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_start_query
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_query(
        string_view query_string,
        execution_state& st,
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Prepares a statement server-side.
     * \details
     * After this operation completes, `result` will reference `*this`.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     *\n
     * `stmt` should be encoded using the connection's character set.
     */
    void prepare_statement(string_view stmt, statement<Stream>& result, error_code&, server_diagnostics&);

    /// \copydoc prepare_statement
    void prepare_statement(string_view stmt, statement<Stream>& result);

    /**
     * \copydoc prepare_statement
     * \details
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `stmt` <b>must be kept alive by the caller until the operation is
     * initiated</b>.
     *\n
     * The handler signature for this operation is `void(boost::mysql::error_code)`
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_prepare_statement(
        string_view stmt,
        statement<Stream>& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_prepare_statement(stmt, result, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_prepare_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_prepare_statement(
        string_view stmt,
        statement<Stream>& result,
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Reads a single row.
     * \details
     * If a row was read successfully, returns a non-empty \ref row_view.
     * If there were no more rows to read, returns an empty `row_view`.
     *\n
     * The returned view points into memory owned by `*this`. It will be valid until the
     * underlying stream performs any other read operation or is destroyed.
     *\n
     * `st` must have previously been populated by a function starting the multifunction
     * operation, like \ref start_query or \ref statement::start_execution. Otherwise, the results
     * are undefined.
     */
    row_view read_one_row(execution_state& st, error_code& err, server_diagnostics& info);

    /// \copydoc read_one_row
    row_view read_one_row(execution_state& st);

    /**
     * \copydoc read_one_row
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::row_view)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::row_view))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, row_view))
    async_read_one_row(
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_one_row(st, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_read_one_row
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::row_view))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, row_view))
    async_read_one_row(
        execution_state& st,
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Reads a batch of rows.
     * \details
     * The number of rows that will be read is unspecified. If the resultset being read
     * has still rows to read, at least one will be read. If there are no more
     * rows to be read, returns an empty `rows_view`.
     * \n
     * The number of rows that will be read depends on the input buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in \ref
     * connection::connect, using \ref buffer_params::initial_read_buffer_size. The buffer may be
     * grown bigger by other read operations, if required.
     * \n
     * The returned view points into memory owned by `*this`. It will be valid until the
     * underlying stream performs any other read operation or is destroyed.
     */
    rows_view read_some_rows(execution_state& st, error_code& err, server_diagnostics& info);

    /// \copydoc read_some_rows
    rows_view read_some_rows(execution_state& st);

    /**
     * \copydoc read_some_rows
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::rows_view)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_some_rows(
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_some_rows(st, shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_read_some_rows
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_some_rows(
        execution_state& st,
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Closes the connection to the server.
     * \details
     * This function is only available if `Stream` satisfies the `SocketStream` concept.
     *\n
     * Sends a quit request, performs the TLS shutdown (if required)
     * and closes the underlying stream. Prefer this function to \ref connection::quit.
     *\n
     * After calling this function, any \ref statement referencing `*this` will
     * no longer be usable for server interaction.
     *\n
     */
    void close(error_code&, server_diagnostics&);

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
        return async_close(this->shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Notifies the MySQL server that the client wants to end the session and shutdowns SSL.
     * \details Sends a quit request to the MySQL server. If the connection is using SSL,
     * this function will also perform the SSL shutdown. You should
     * close the underlying physical connection after calling this function.
     *\n
     * After calling this function, any \ref statement referencing `*this` will
     * no longer be usable for server interaction.
     *\n
     * If the `Stream` template parameter fulfills the `SocketConnection`
     * requirements, use \ref connection::close instead of this function,
     * as it also takes care of closing the underlying stream.
     */
    void quit(error_code&, server_diagnostics&);

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
        return async_quit(shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_quit
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_quit(
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Rebinds the connection type to another executor.
     * \details The Stream type must either provide a `rebind_executor`
     * member with the same semantics, or be an instantiation of `boost::asio::ssl::stream` with
     * a Stream type providing a `rebind_executor` member.
     */
    template <typename Executor1>
    struct rebind_executor
    {
        /// The connection type when rebound to the specified executor.
        using other = connection<typename detail::rebind_executor<Stream, Executor1>::type>;
    };
};

/// The default TCP port for the MySQL protocol.
constexpr unsigned short default_port = 3306;

/// The default TCP port for the MySQL protocol, as a string. Useful for hostname resolution.
constexpr const char* default_port_string = "3306";

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/connection.hpp>

#endif
