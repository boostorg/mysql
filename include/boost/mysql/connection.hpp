//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_HPP
#define BOOST_MYSQL_CONNECTION_HPP

#include <boost/mysql/buffer_params.hpp>
#include <boost/mysql/connection_base.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/any_stream.hpp>
#include <boost/mysql/detail/any_stream_impl.hpp>
#include <boost/mysql/detail/connection_impl.hpp>
#include <boost/mysql/detail/execution_concepts.hpp>
#include <boost/mysql/detail/rebind_executor.hpp>
#include <boost/mysql/detail/socket_stream.hpp>
#include <boost/mysql/detail/throw_on_error_loc.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/assert.hpp>

#include <memory>
#include <type_traits>
#include <utility>

/// The Boost libraries namespace.
namespace boost {
/// Boost.MySQL library namespace.
namespace mysql {

/**
 * \brief A connection to a MySQL server.
 * \details
 * Represents a connection to a MySQL server.
 *\n
 * `connection` is the main I/O object that this library implements. It owns a `Stream` object that
 * is accessed by functions involving network operations, as well as session state. You can access
 * the stream using \ref connection::stream, and its executor via \ref connection::get_executor. The
 * executor used by this object is always the same as the underlying stream.
 *\n
 * \par Thread safety
 * Distinct objects: safe. \n
 * Shared objects: unsafe. \n
 * This class is <b>not thread-safe</b>: for a single object, if you
 * call its member functions concurrently from separate threads, you will get a race condition.
 */
template <class Stream>
class connection : public connection_base<typename Stream::executor_type>
{
#ifndef BOOST_MYSQL_DOXYGEN
    using base_type = connection_base<typename Stream::executor_type>;
    friend struct detail::access;
#endif

public:
    /**
     * \brief Initializing constructor.
     * \details
     * As part of the initialization, an internal `Stream` object is created.
     *
     * \par Exception safety
     * Basic guarantee. Throws if the `Stream` constructor throws
     * or if memory allocation for internal state fails.
     *
     * \param args Arguments to be forwarded to the `Stream` constructor.
     */
    template <
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type>
    connection(Args&&... args) : connection(buffer_params(), std::forward<Args>(args)...)
    {
    }

    /**
     * \brief Initializing constructor with buffer params.
     * \details
     * As part of the initialization, an internal `Stream` object is created.
     *
     * \par Exception safety
     * Basic guarantee. Throws if the `Stream` constructor throws
     * or if memory allocation for internal state fails.
     *
     * \param buff_params Specifies initial sizes for internal buffers.
     * \param args Arguments to be forwarded to the `Stream` constructor.
     */
    template <
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type>
    connection(const buffer_params& buff_params, Args&&... args)
        : base_type(buff_params, detail::make_stream<Stream>(std::forward<Args>(args)...))
    {
    }

    /**
     * \brief Move constructor.
     */
    connection(connection&& other) = default;

    /**
     * \brief Move assignment.
     */
    connection& operator=(connection&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
#endif

    /// The executor type associated to this object.
    using executor_type = typename Stream::executor_type;

    /// Retrieves the executor associated to this object.
    executor_type get_executor() { return stream().get_executor(); }

    /// The `Stream` type this connection is using.
    using stream_type = Stream;

    /**
     * \brief Retrieves the underlying Stream object.
     * \details
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    Stream& stream() noexcept { return detail::cast<Stream>(this->impl_.stream()); }

    /**
     * \brief Retrieves the underlying Stream object.
     * \details
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    const Stream& stream() const noexcept { return detail::cast<Stream>(this->impl_.stream()); }

    /**
     * \brief Establishes a connection to a MySQL server.
     * \details
     * This function is only available if `Stream` satisfies the
     * `SocketStream` concept.
     * \n
     * Connects the underlying stream and performs the handshake
     * with the server. The underlying stream is closed in case of error. Prefer
     * this function to \ref connection::handshake.
     * \n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * \n
     * `endpoint` should be convertible to `Stream::lowest_layer_type::endpoint_type`.
     */
    template <typename EndpointType>
    void connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        error_code& ec,
        diagnostics& diag
    )
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "connect can only be used if Stream satisfies the SocketStream concept"
        );
        this->impl_.template connect<Stream>(endpoint, params, ec, diag);
    }

    /// \copydoc connect
    template <typename EndpointType>
    void connect(const EndpointType& endpoint, const handshake_params& params)
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "connect can only be used if Stream satisfies the SocketStream concept"
        );
        error_code err;
        diagnostics diag;
        connect(endpoint, params, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc connect
     * \par Object lifetimes
     * The strings pointed to by `params` should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     * `endpoint` is copied as required and doesn't need to be kept alive.
     *
     * \par Handler signature
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
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "async_connect can only be used if Stream satisfies the SocketStream concept"
        );
        return async_connect(
            endpoint,
            params,
            this->impl_.shared_diag(),
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
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "async_connect can only be used if Stream satisfies the SocketStream concept"
        );
        return this->impl_
            .template async_connect<Stream>(endpoint, params, diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Performs the MySQL-level handshake.
     * \details
     * Does not connect the underlying stream.
     * If the `Stream` template parameter fulfills the `SocketConnection`
     * requirements, use \ref connection::connect instead of this function.
     * \n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     */
    void handshake(const handshake_params& params, error_code& ec, diagnostics& diag)
    {
        this->impl_.run(this->impl_.make_params_handshake(params, diag), ec);
    }

    /// \copydoc handshake
    void handshake(const handshake_params& params)
    {
        error_code err;
        diagnostics diag;
        handshake(params, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc handshake
     * \par Object lifetimes
     * The strings pointed to by `params` should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     *
     * \par Handler signature
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
        return async_handshake(params, this->impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_handshake
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_handshake(
        const handshake_params& params,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->impl_.async_run(
            this->impl_.make_params_handshake(params, diag),
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief (Deprecated) Executes a SQL text query.
     * \details
     * Sends `query_string` to the server for execution and reads the response into `result`.
     * query_string should be encoded using the connection's character set.
     * \n
     * After this operation completes successfully, `result.has_value() == true`.
     * \n
     * Metadata in `result` will be populated according to `this->meta_mode()`.
     * \n
     * \par Security
     * If you compose `query_string` by concatenating strings manually, <b>your code is
     * vulnerable to SQL injection attacks</b>. If your query contains patameters unknown at
     * compile time, use prepared statements instead of this function.
     *
     * \par Deprecation notice
     * This function is only provided for backwards-compatibility. For new code, please
     * use \ref execute or \ref async_execute instead.
     */
    void query(string_view query_string, results& result, error_code& err, diagnostics& diag)
    {
        this->execute(query_string, result, err, diag);
    }

    /// \copydoc query
    void query(string_view query_string, results& result) { this->execute(query_string, result); }

    /**
     * \copydoc query
     * \details
     * \par Object lifetimes
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `query_string` must be kept alive by the caller until the operation is
     * initiated.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        string_view query_string,
        results& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->async_query(
            query_string,
            result,
            this->impl_.shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_query
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_query(
        string_view query_string,
        results& result,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->async_execute(query_string, result, diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief (Deprecated) Starts a text query as a multi-function operation.
     * \details
     * Writes the query request and reads the initial server response and the column
     * metadata, but not the generated rows or subsequent resultsets, if any.
     * After this operation completes, `st` will have
     * \ref execution_state::meta populated.
     * Metadata will be populated according to `this->meta_mode()`.
     * \n
     * If the operation generated any rows or more than one resultset, these <b>must</b> be read (by using
     * \ref read_some_rows and \ref read_resultset_head) before engaging in any further network operation.
     * Otherwise, the results are undefined.
     * \n
     * `query_string` should be encoded using the connection's character set.
     *
     * \par Deprecation notice
     * This function is only provided for backwards-compatibility. For new code, please
     * use \ref start_execution or \ref async_start_execution instead.
     */
    void start_query(string_view query_string, execution_state& st, error_code& err, diagnostics& diag)
    {
        this->start_execution(query_string, st, err, diag);
    }

    /// \copydoc start_query
    void start_query(string_view query_string, execution_state& st)
    {
        this->start_execution(query_string, st);
    }

    /**
     * \copydoc start_query
     * \details
     * \par Object lifetimes
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `query_string` must be kept alive by the caller until the operation is
     * initiated.
     *
     * \par Handler signature
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
        return this->async_start_query(
            query_string,
            st,
            this->impl_.shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_start_query
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_query(
        string_view query_string,
        execution_state& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->async_start_execution(query_string, st, diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief (Deprecated) Executes a prepared statement.
     * \details
     * Executes a statement with the given parameters and reads the response into `result`.
     * \n
     * After this operation completes successfully, `result.has_value() == true`.
     * \n
     * The statement actual parameters (`params`) are passed as a `std::tuple` of elements.
     * See the `WritableFieldTuple` concept defition for more info. You should pass exactly as many
     * parameters as `this->num_params()`, or the operation will fail with an error.
     * String parameters should be encoded using the connection's character set.
     * \n
     * Metadata in `result` will be populated according to `conn.meta_mode()`, where `conn`
     * is the connection that prepared this statement.
     *
     * \par Deprecation notice
     * This function is only provided for backwards-compatibility. For new code, please
     * use \ref execute or \ref async_execute instead.
     *
     * \par Preconditions
     *    `stmt.valid() == true`
     */
    template <
        BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
        class EnableIf =
            typename std::enable_if<detail::is_writable_field_tuple<WritableFieldTuple>::value>::type>
    void execute_statement(
        const statement& stmt,
        const WritableFieldTuple& params,
        results& result,
        error_code& err,
        diagnostics& diag
    )
    {
        this->execute(stmt.bind(params), result, err, diag);
    }

    /// \copydoc execute_statement
    template <
        BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
        class EnableIf =
            typename std::enable_if<detail::is_writable_field_tuple<WritableFieldTuple>::value>::type>
    void execute_statement(const statement& stmt, const WritableFieldTuple& params, results& result)
    {
        this->execute(stmt.bind(params), result);
    }

    /**
     * \copydoc execute_statement
     * \par Object lifetimes
     * If `CompletionToken` is deferred (like `use_awaitable`), and `params` contains any reference
     * type (like `string_view`), the caller must keep the values pointed by these references alive
     * until the operation is initiated. Value types will be copied/moved as required, so don't need
     * to be kept alive. It's not required to keep `stmt` alive, either.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf =
            typename std::enable_if<detail::is_writable_field_tuple<WritableFieldTuple>::value>::type>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute_statement(
        const statement& stmt,
        WritableFieldTuple&& params,
        results& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->async_execute_statement(
            stmt,
            std::forward<WritableFieldTuple>(params),
            result,
            this->impl_.shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_execute_statement
    template <
        BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf =
            typename std::enable_if<detail::is_writable_field_tuple<WritableFieldTuple>::value>::type>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute_statement(
        const statement& stmt,
        WritableFieldTuple&& params,
        results& result,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->async_execute(
            stmt.bind(std::forward<WritableFieldTuple>(params)),
            result,
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief (Deprecated) Starts a statement execution as a multi-function operation.
     * \details
     * Writes the execute request and reads the initial server response and the column
     * metadata, but not the generated rows or subsequent resultsets, if any. After this operation completes,
     * `st` will have \ref execution_state::meta populated. Metadata will be populated according to
     * `this->meta_mode()`.
     * \n
     * If the operation generated any rows or more than one resultset, these <b>must</b> be read (by using
     * \ref read_some_rows and \ref read_resultset_head) before engaging in any further network operation.
     * Otherwise, the results are undefined.
     * \n
     * The statement actual parameters (`params`) are passed as a `std::tuple` of elements.
     * String parameters should be encoded using the connection's character set.
     *
     * \par Deprecation notice
     * This function is only provided for backwards-compatibility. For new code, please
     * use \ref start_execution or \ref async_start_execution instead.
     *
     * \par Preconditions
     *    `stmt.valid() == true`
     */
    template <
        BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
        class EnableIf =
            typename std::enable_if<detail::is_writable_field_tuple<WritableFieldTuple>::value>::type>
    void start_statement_execution(
        const statement& stmt,
        const WritableFieldTuple& params,
        execution_state& st,
        error_code& err,
        diagnostics& diag
    )
    {
        this->start_execution(stmt.bind(params), st, err, diag);
    }

    /// \copydoc start_statement_execution(const statement&,const WritableFieldTuple&,execution_state&,error_code&,diagnostics&)
    template <
        BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
        class EnableIf =
            typename std::enable_if<detail::is_writable_field_tuple<WritableFieldTuple>::value>::type>
    void start_statement_execution(
        const statement& stmt,
        const WritableFieldTuple& params,
        execution_state& st
    )
    {
        this->start_execution(stmt.bind(params), st);
    }

    /**
     * \copydoc start_statement_execution(const statement&,const WritableFieldTuple&,execution_state&,error_code&,diagnostics&)
     * \details
     * \par Object lifetimes
     * If `CompletionToken` is deferred (like `use_awaitable`), and `params` contains any reference
     * type (like `string_view`), the caller must keep the values pointed by these references alive
     * until the operation is initiated. Value types will be copied/moved as required, so don't need
     * to be kept alive. It's not required to keep `stmt` alive, either.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf =
            typename std::enable_if<detail::is_writable_field_tuple<WritableFieldTuple>::value>::type>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_statement_execution(
        const statement& stmt,
        WritableFieldTuple&& params,
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->async_start_statement_execution(
            stmt,
            std::forward<WritableFieldTuple>(params),
            st,
            this->impl_.shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_start_statement_execution(const statement&,WritableFieldTuple&&,execution_state&,CompletionToken&&)
    template <
        BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf =
            typename std::enable_if<detail::is_writable_field_tuple<WritableFieldTuple>::value>::type>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_statement_execution(
        const statement& stmt,
        WritableFieldTuple&& params,
        execution_state& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->async_start_execution(
            stmt.bind(std::forward<WritableFieldTuple>(params)),
            st,
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief (Deprecated) Starts a statement execution as a multi-function operation.
     * \details
     * Writes the execute request and reads the initial server response and the column
     * metadata, but not the generated rows or any subsequent resultsets, if any. After this operation
     * completes, `st` will have \ref execution_state::meta populated.
     * \n
     * If the operation generated any rows or more than one resultset, these <b>must</b> be read (by using
     * \ref read_some_rows and \ref read_resultset_head) before engaging in any further network operation.
     * Otherwise, the results are undefined.
     * \n
     * The statement actual parameters are passed as an iterator range.
     * String parameters should be encoded using the connection's character set.
     *
     * \par Deprecation notice
     * This function is only provided for backwards-compatibility. For new code, please
     * use \ref start_execution or \ref async_start_execution instead.
     *
     * \par Preconditions
     *    `stmt.valid() == true`
     */
    template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
    void start_statement_execution(
        const statement& stmt,
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st,
        error_code& ec,
        diagnostics& diag
    )
    {
        this->start_execution(stmt.bind(params_first, params_last), st, ec, diag);
    }

    /// \copydoc start_statement_execution(const statement&,FieldViewFwdIterator,FieldViewFwdIterator,execution_state&,error_code&,diagnostics&)
    template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
    void start_statement_execution(
        const statement& stmt,
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st
    )
    {
        this->start_execution(stmt.bind(params_first, params_last), st);
    }

    /**
     * \copydoc start_statement_execution(const statement&,FieldViewFwdIterator,FieldViewFwdIterator,execution_state&,error_code&,diagnostics&)
     * \details
     * \par Object lifetimes
     * If `CompletionToken` is deferred (like `use_awaitable`), the caller must keep objects in
     * the iterator range alive until the  operation is initiated.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_statement_execution(
        const statement& stmt,
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->async_start_statement_execution(
            stmt,
            params_first,
            params_last,
            st,
            this->impl_.shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_start_statement_execution(const statement&,FieldViewFwdIterator,FieldViewFwdIterator,execution_state&,CompletionToken&&)
    template <
        BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_statement_execution(
        const statement& stmt,
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return this->async_start_execution(
            stmt.bind(params_first, params_last),
            st,
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Closes the connection to the server.
     * \details
     * This function is only available if `Stream` satisfies the `SocketStream` concept.
     * \n
     * Sends a quit request, performs the TLS shutdown (if required)
     * and closes the underlying stream. Prefer this function to \ref connection::quit.
     */
    void close(error_code& err, diagnostics& diag)
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "close can only be used if Stream satisfies the SocketStream concept"
        );
        this->impl_.run(this->impl_.make_params_close(diag), err);
    }

    /// \copydoc close
    void close()
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "close can only be used if Stream satisfies the SocketStream concept"
        );
        error_code err;
        diagnostics diag;
        close(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc close
     * \details
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "async_close can only be used if Stream satisfies the SocketStream concept"
        );
        return this->async_close(this->impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(diagnostics& diag, CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "async_close can only be used if Stream satisfies the SocketStream concept"
        );
        return this->impl_.async_run(
            this->impl_.make_params_close(diag),
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Notifies the MySQL server that the client wants to end the session and shutdowns SSL.
     * \details Sends a quit request to the MySQL server. If the connection is using SSL,
     * this function will also perform the SSL shutdown. You should
     * close the underlying physical connection after calling this function.
     * \n
     * If the `Stream` template parameter fulfills the `SocketConnection`
     * requirements, use \ref connection::close instead of this function,
     * as it also takes care of closing the underlying stream.
     */
    void quit(error_code& err, diagnostics& diag)
    {
        this->impl_.run(this->impl_.make_params_quit(diag), err);
    }

    /// \copydoc quit
    void quit()
    {
        error_code err;
        diagnostics diag;
        quit(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc quit
     * \details
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_quit(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_quit(this->impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_quit
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_quit(diagnostics& diag, CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return this->impl_.async_run(
            this->impl_.make_params_quit(diag),
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Rebinds the connection type to another executor.
     * \details
     * The `Stream` type must either provide a `rebind_executor`
     * member with the same semantics, or be an instantiation of `boost::asio::ssl::stream` with
     * a `Stream` type providing a `rebind_executor` member.
     */
    template <typename Executor1>
    struct rebind_executor
    {
        /// The connection type when rebound to the specified executor.
        using other = connection<typename detail::rebind_executor<Stream, Executor1>::type>;
    };
};

}  // namespace mysql
}  // namespace boost

#endif
