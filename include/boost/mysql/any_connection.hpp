//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ANY_CONNECTION_HPP
#define BOOST_MYSQL_ANY_CONNECTION_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/defaults.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/execution_concepts.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/capy/concept/executor.hpp>
#include <boost/capy/ex/execution_context.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/result.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace boost {
namespace mysql {

// Forward declarations
template <class... StaticRow>
class static_execution_state;

class execution_state;
class pipeline_request;
class stage_response;

/**
 * \brief Configuration parameters that can be passed to \ref any_connection's constructor.
 */
struct any_connection_params
{
    // /**
    //  * \brief An external SSL context containing options to configure TLS.
    //  * \details
    //  * Relevant only for SSL connections (those that result on \ref
    //  * any_connection::uses_ssl returning `true`).
    //  * \n
    //  * If the connection is configured to use TLS, an internal `asio::ssl::stream`
    //  * object will be created. If this member is set to a non-null value,
    //  * this internal object will be initialized using the passed context.
    //  * This is the only way to configure TLS options in `any_connection`.
    //  * \n
    //  * If the connection is configured to use TLS and this member is `nullptr`,
    //  * an internal `asio::ssl::context` object with suitable default options
    //  * will be created.
    //  *
    //  * \par Object lifetimes
    //  * If set to non-null, the pointee object must be kept alive until
    //  * all \ref any_connection objects constructed from `*this` are destroyed.
    //  */
    // asio::ssl::context* ssl_context{};

    /**
     * \brief The initial size of the connection's buffer, in bytes.
     * \details A bigger read buffer can increase the number of rows
     * returned by \ref any_connection::read_some_rows.
     */
    std::size_t initial_buffer_size{default_initial_read_buffer_size};

    /**
     * \brief The maximum size of the connection's buffer, in bytes (64MB by default).
     * \details
     * Attempting to read or write a protocol packet bigger than this size
     * will fail with a \ref client_errc::max_buffer_size_exceeded error.
     * \n
     * This effectively means: \n
     *   - Each request sent to the server must be smaller than this value.
     *   - Each individual row received from the server must be smaller than this value.
     *     Note that when using `execute` or `async_execute`, results objects may
     *     allocate memory beyond this limit if the total number of rows is high.
     * \n
     * If you need to send or receive larger packets, you may need to adjust
     * your server's <a
     * href="https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html#sysvar_max_allowed_packet">`max_allowed_packet`</a>
     * system variable, too.
     */
    std::size_t max_buffer_size{0x4000000};
};

/**
 * \brief A connection to a MySQL server.
 * \details
 * Represents a connection to a MySQL server.
 * This is the main I/O object that this library implements. It's logically comprised
 * of session state and an internal stream (usually a socket). The stream is not directly
 * accessible. It's constructed using the executor passed to the constructor.
 *
 * This class supports establishing connections
 * with servers using TCP, TCP over TLS and UNIX sockets.
 *
 * The class is named `any_connection` because it's not templated on a `Stream`
 * type, as opposed to \ref connection. New code should prefer using `any_connection`
 * whenever possible.
 *
 * Compared to \ref connection, this class:
 *
 * - Is type-erased. The type of the connection doesn't depend on the transport being used.
 * - Is easier to connect, as \ref connect and \ref async_connect handle hostname resolution.
 * - Can always be re-connected after being used or encountering an error.
 * - Always uses `asio::any_io_executor`.
 * - Has the same level of performance.
 *
 * This is a move-only type.
 *
 * \par Single outstanding async operation per connection
 * At any given point in time, only one async operation can be outstanding
 * per connection. If an async operation is initiated while another one is in progress,
 * it will fail with \ref client_errc::operation_in_progress.
 *
 * \par Default completion tokens
 * The default completion token for all async operations in this class is
 * `with_diagnostics(asio::deferred)`, which allows you to use `co_await`
 * and have the expected exceptions thrown on error.
 *
 * \par Thread safety
 * Distinct objects: safe. \n
 * Shared objects: unsafe. \n
 * This class is <b>not thread-safe</b>: for a single object, if you
 * call its member functions concurrently from separate threads, you will get a race condition.
 */
class any_connection
{
    struct impl_t;
    std::unique_ptr<impl_t> impl_;

    // Private helpers for template functions
    std::vector<field_view>& shared_fields();

    // Non-template I/O impl functions
    capy::io_task<diagnostics> start_execution_impl(
        detail::any_execution_request req,
        detail::execution_processor* proc
    );
    capy::io_task<diagnostics, std::size_t> read_some_rows_impl(
        detail::execution_processor* proc,
        detail::output_ref output
    );
    capy::io_task<diagnostics> read_resultset_head_impl(detail::execution_processor* proc);

public:
    /**
     * \brief Constructs a connection object from an executor and an optional set of parameters.
     * \details
     * The resulting connection has `this->get_executor() == ex`. Any internally required I/O objects
     * will be constructed using this executor.
     * \n
     * You can configure extra parameters, like the SSL context and buffer sizes, by passing
     * an \ref any_connection_params object to this constructor.
     */
    template <capy::Executor Ex>
    any_connection(const Ex& ex, any_connection_params params = {}) : any_connection(ex.context(), params)
    {
    }

    /**
     * \brief Constructs a connection object from an execution context and an optional set of parameters.
     * \details
     * The resulting connection has `this->get_executor() == ctx.get_executor()`.
     * Any internally required I/O objects will be constructed using this executor.
     * \n
     * You can configure extra parameters, like the SSL context and buffer sizes, by passing
     * an \ref any_connection_params object to this constructor.
     * \n
     * This function participates in overload resolution only if `ExecutionContext`
     * satisfies the `ExecutionContext` requirements imposed by Boost.Asio.
     */
    any_connection(capy::execution_context& ctx, any_connection_params params = {});

    /**
     * \brief Move constructor.
     */
    any_connection(any_connection&& other) noexcept = default;

    /**
     * \brief Move assignment.
     */
    any_connection& operator=(any_connection&& rhs) noexcept;

#ifndef BOOST_MYSQL_DOXYGEN
    any_connection(const any_connection&) = delete;
    any_connection& operator=(const any_connection&) = delete;
#endif

    /**
     * \brief Destructor.
     * \details
     * Closes the connection at the transport layer (by closing any underlying socket objects).
     * If you require a clean close, call \ref close or \ref async_close before the connection
     * is destroyed.
     */
    ~any_connection();

    /**
     * \brief Returns whether the connection negotiated the use of SSL or not.
     * \details
     * This function can be used to determine whether you are using a SSL
     * connection or not when using SSL negotiation.
     * \n
     * This function always returns `false`
     * for connections that haven't been established yet. If the connection establishment fails,
     * the return value is undefined.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool uses_ssl() const noexcept;

    /**
     * \brief Returns whether backslashes are being treated as escape sequences.
     * \details
     * By default, the server treats backslashes in string values as escape characters.
     * This behavior can be disabled by activating the <a
     *   href="https://dev.mysql.com/doc/refman/8.0/en/sql-mode.html#sqlmode_no_backslash_escapes">`NO_BACKSLASH_ESCAPES`</a>
     * SQL mode.
     * \n
     * Every time an operation involving server communication completes, the server reports whether
     * this mode was activated or not as part of the response. Connections store this information
     * and make it available through this function.
     * \n
     * \li If backslash are treated like escape characters, returns `true`.
     * \li If `NO_BACKSLASH_ESCAPES` has been activated, returns `false`.
     * \li If connection establishment hasn't happened yet, returns `true`.
     * \li Calling this function while an async operation that changes backslash behavior
     *     is outstanding may return `true` or `false`.
     * \n
     * This function does not involve server communication.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool backslash_escapes() const noexcept;

    /**
     * \brief Returns the character set used by this connection.
     * \details
     * Connections attempt to keep track of the current character set.
     * Deficiencies in the protocol can cause the character set to be unknown, though.
     * When the character set is known, this function returns
     * the character set currently in use. Otherwise, returns \ref client_errc::unknown_character_set.
     * \n
     * The following functions can modify the return value of this function: \n
     *   \li Prior to connection, the character set is always unknown.
     *   \li \ref connect may set the current character set
     *       to a known value, depending on the requested collation.
     *   \li \ref set_character_set always
     *       sets the current character set to the passed value.
     *   \li \ref reset_connection always makes the current character
     *       unknown.
     *
     * \par Avoid changing the character set directly
     * If you change the connection's character set directly using SQL statements
     * like `"SET NAMES utf8mb4"`, the client has no way to track this change,
     * and this function will return incorrect results.
     *
     * \par Errors
     * \li \ref client_errc::unknown_character_set if the current character set is unknown.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    system::result<character_set> current_character_set() const noexcept;

    /**
     * \brief Returns format options suitable to format SQL according to the current connection
     * configuration.
     * \details
     * If the current character set is known (as given by \ref current_character_set), returns
     * a value suitable to be passed to SQL formatting functions. Otherwise, returns an error.
     *
     * \par Errors
     * \li \ref client_errc::unknown_character_set if the current character set is unknown.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    system::result<format_options> format_opts() const noexcept;

    /**
     * \brief Returns the current metadata mode that this connection is using.
     * \details
     * \par Exception safety
     * No-throw guarantee.
     *
     * \returns The metadata mode that will be used for queries and statement executions.
     */
    metadata_mode meta_mode() const noexcept;

    /**
     * \brief Sets the metadata mode.
     * \details
     * Will affect any query and statement executions performed after the call.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Preconditions
     * No asynchronous operation should be outstanding when this function is called.
     *
     * \param v The new metadata mode.
     */
    void set_meta_mode(metadata_mode v) noexcept;

    /**
     * \brief Retrieves the connection id associated to the current session.
     * \details
     * If a session has been established, returns its associated connection id.
     * If no session has been established (i.e. \ref connect hasn't been called yet)
     * or the session has been terminated (i.e. \ref close has been called), an empty
     * optional is returned.
     *
     * The connection id is a 4 byte value that uniquely identifies a client session
     * at a given point in time. It can be used with the
     * <a href="https://dev.mysql.com/doc/refman/8.4/en/kill.html">`KILL`</a> SQL statement
     * to cancel queries and terminate connections.
     *
     * The server sends the connection id assigned to the current session as part of the
     * handshake process. The value is stored and made available through this function.
     * The same id can also be obtained by calling the
     * <a
     * href="https://dev.mysql.com/doc/refman/8.4/en/information-functions.html#function_connection-id">CONNECTION_ID()</a>
     * SQL function. However, this function is faster and more reliable, since it does not entail
     * communication with the server.
     *
     * This function is equivalent to the
     * <a href="https://dev.mysql.com/doc/c-api/8.0/en/mysql-thread-id.html">`mysql_thread_id`</a> function
     * in the C connector. This function works properly in 64-bit systems, as opposed to what
     * the official docs suggest (see
     * <a href="https://dev.mysql.com/doc/relnotes/mysql/5.7/en/news-5-7-5.html">this changelog</a>).
     *
     * It is safe to call this function while an async operation is outstanding, except for \ref
     * connect and \ref close.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    boost::optional<std::uint32_t> connection_id() const noexcept;

    /**
     * \brief Establishes a connection to a MySQL server.
     * \details
     * This function performs the following:
     * \n
     * \li If a connection has already been established (by a previous call to \ref connect
     *     or \ref async_connect), closes it at the transport layer (by closing any underlying socket)
     *     and discards any protocol state associated to it. (If you require
     *     a clean close, call \ref close or \ref async_close before using this function).
     * \li If the connection is configured to use TCP (`params.server_address.type() ==
     *     address_type::host_and_port`), resolves the passed hostname to a set of endpoints. An empty
     *     hostname is equivalent to `"localhost"`.
     * \li Establishes the physical connection (performing the
     *     TCP or UNIX socket connect).
     * \li Performs the MySQL handshake to establish a session. If the
     *     connection is configured to use TLS, the TLS handshake is performed as part of this step.
     * \li If any of the above steps fail, the TCP or UNIX socket connection is closed.
     * \n
     * You can configure some options using the \ref connect_params struct.
     * \n
     * The decision to use TLS or not is performed using the following:
     * \n
     * \li If the transport is not TCP (`params.server_address.type() != address_type::host_and_port`),
     *     the connection will never use TLS.
     * \li If the transport is TCP, and `params.ssl == ssl_mode::disable`, the connection will not use TLS.
     * \li If the transport is TCP, and `params.ssl == ssl_mode::enable`, the connection will use TLS
     *     only if the server supports it.
     * \li If the transport is TCP, and `params.ssl == ssl_mode::require`, the connection will always use TLS.
     *     If the server doesn't support it, the operation will fail with \ref
     *     client_errc::server_doesnt_support_ssl.
     * \n
     * If `params.connection_collation` is within a set of well-known collations, this function
     * sets the current character set, such that \ref current_character_set returns a non-null value.
     * The default collation (`utf8mb4_general_ci`) is the only one guaranteed to be in the set of well-known
     * collations.
     */
    capy::io_task<diagnostics> connect(const connect_params& params);

    /**
     * \brief Executes a text query or prepared statement.
     * \details
     * Sends `req` to the server for execution and reads the response into `result`.
     * `result` may be either a \ref results or \ref static_results object.
     * `req` should may be either a type convertible to \ref string_view containing valid SQL
     * or a bound prepared statement, obtained by calling \ref statement::bind.
     * If a string, it must be encoded using the connection's character set.
     * Any string parameters provided to \ref statement::bind should also be encoded
     * using the connection's character set.
     * \n
     * After this operation completes successfully, `result.has_value() == true`.
     * \n
     * Metadata in `result` will be populated according to `this->meta_mode()`.
     */
    template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_RESULTS_TYPE ResultsType>
    capy::io_task<diagnostics> execute(ExecutionRequest&& req, ResultsType& result)
    {
        auto req_ref = detail::execution_request_traits<std::decay_t<ExecutionRequest>>::make_request(
            std::forward<ExecutionRequest>(req),
            shared_fields()
        );
        auto* results_ref = &detail::access::get_impl(result).get_interface();
        co_return co_await execute_impl(req_ref, results_ref);
    }

    capy::io_task<diagnostics> execute_impl(
        detail::any_execution_request req,
        detail::execution_processor* proc
    );

    /**
     * \brief Prepares a statement server-side.
     * \details
     * `stmt` should be encoded using the connection's character set.
     * \n
     * The returned statement has `valid() == true`.
     */
    capy::io_task<diagnostics, statement> prepare_statement(std::string_view stmt);

    /**
     * \brief Closes a statement, deallocating it from the server.
     * \details
     * After this operation succeeds, `stmt` must not be used again for execution.
     * \n
     * \par Preconditions
     *    `stmt.valid() == true`
     */
    capy::io_task<diagnostics> close_statement(const statement& stmt);

    /**
     * \brief Starts a SQL execution as a multi-function operation.
     * \details
     * Writes the execution request and reads the initial server response and the column
     * metadata, but not the generated rows or subsequent resultsets, if any.
     * `st` may be either an \ref execution_state or \ref static_execution_state object.
     * \n
     * After this operation completes, `st` will have
     * \ref execution_state::meta populated.
     * Metadata will be populated according to `this->meta_mode()`.
     * \n
     * If the operation generated any rows or more than one resultset, these <b>must</b> be read (by using
     * \ref read_some_rows and \ref read_resultset_head) before engaging in any further network operation.
     * Otherwise, the results are undefined.
     * \n
     * req may be either a type convertible to \ref string_view containing valid SQL
     * or a bound prepared statement, obtained by calling \ref statement::bind.
     * If a string, it must be encoded using the connection's character set.
     * Any string parameters provided to \ref statement::bind should also be encoded
     * using the connection's character set.
     * \n
     * When using the static interface, this function will detect schema mismatches for the first
     * resultset. Further errors may be detected by \ref read_resultset_head and \ref read_some_rows.
     */
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
    capy::io_task<diagnostics> start_execution(ExecutionRequest&& req, ExecutionStateType& st)
    {
        auto req_ref = detail::execution_request_traits<std::decay_t<ExecutionRequest>>::make_request(
            std::forward<ExecutionRequest>(req),
            shared_fields()
        );
        co_return co_await start_execution_impl(req_ref, &detail::access::get_impl(st).get_interface());
    }

    /**
     * \brief Reads a batch of rows.
     * \details
     * The number of rows that will be read is unspecified. If the operation represented by `st`
     * has still rows to read, at least one will be read. If there are no more rows, or
     * `st.should_read_rows() == false`, returns an empty `rows_view`.
     * \n
     * The number of rows that will be read depends on the connection's buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in the
     * constructor. The buffer may be
     * grown bigger by other read operations, if required.
     * \n
     * The returned view points into memory owned by `*this`. It will be valid until
     * `*this` performs the next network operation or is destroyed.
     */
    capy::io_task<diagnostics, rows_view> read_some_rows(execution_state& st);

    /**
     * \brief Reads a batch of rows.
     * \details
     * Reads a batch of rows of unspecified size into the storage given by `output`.
     * At most `output.size()` rows will be read. If the operation represented by `st`
     * has still rows to read, and `output.size() > 0`, at least one row will be read.
     * \n
     * Returns the number of read rows.
     * \n
     * If there are no more rows, or `st.should_read_rows() == false`, this function is a no-op and returns
     * zero.
     * \n
     * The number of rows that will be read depends on the connection's buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in the
     * constructor. The buffer may be grown bigger by other read operations, if required.
     * \n
     * Rows read by this function are owning objects, and don't hold any reference to
     * the connection's internal buffers (contrary what happens with the dynamic interface's counterpart).
     * \n
     * \par Extracting rows
     * The type `SpanElementType` must be the underlying row type for one of the types in the
     * `StaticRow` parameter pack (i.e., one of the types in `underlying_row_t<StaticRow>...`).
     * The type must match the resultset that is currently being processed by `st`. For instance,
     * given `static_execution_state<T1, T2>`, when reading rows for the second resultset,
     * `SpanElementType` must exactly be `underlying_row_t<T2>`.
     * If this is not the case, a runtime error will be issued.
     * \n
     * This function can report schema mismatches.
     */
    template <class SpanElementType, class... StaticRow>
    capy::io_task<diagnostics, std::size_t> read_some_rows(
        static_execution_state<StaticRow...>& st,
        std::span<SpanElementType> output
    )
    {
        co_return co_await read_some_rows_impl(
            &detail::access::get_impl(st).get_interface(),
            detail::access::get_impl(st).make_output_ref(output)
        );
    }

    /**
     * \brief Reads metadata for subsequent resultsets in a multi-resultset operation.
     * \details
     * If `st.should_read_head() == true`, this function will read the next resultset's
     * initial response message and metadata, if any. If the resultset indicates a failure
     * (e.g. the query associated to this resultset contained an error), this function will fail
     * with that error.
     * \n
     * If `st.should_read_head() == false`, this function is a no-op.
     * \n
     * `st` may be either an \ref execution_state or \ref static_execution_state object.
     * \n
     * This function is only relevant when using multi-function operations with statements
     * that return more than one resultset.
     * \n
     * When using the static interface, this function will detect schema mismatches for the resultset
     * currently being read. Further errors may be detected by subsequent invocations of this function
     * and by \ref read_some_rows.
     */
    template <BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
    capy::io_task<diagnostics> read_resultset_head(ExecutionStateType& st)
    {
        co_return co_await read_resultset_head_impl(&detail::access::get_impl(st).get_interface());
    }

    /**
     * \brief Sets the connection's character set, as per SET NAMES.
     * \details
     * Sets the connection's character set by running a
     * <a href="https://dev.mysql.com/doc/refman/8.0/en/set-names.html">`SET NAMES`</a>
     * SQL statement, using the passed \ref character_set::name as the charset name to set.
     * \n
     * This function will also update the value returned by \ref current_character_set, so
     * prefer using this function over raw SQL statements.
     * \n
     * If the server was unable to set the character set to the requested value (e.g. because
     * the server does not support the requested charset), this function will fail,
     * as opposed to how \ref connect behaves when an unsupported collation is passed.
     * This is a limitation of MySQL servers.
     * \n
     * You need to perform connection establishment for this function to succeed, since it
     * involves communicating with the server.
     *
     * \par Object lifetimes
     * `charset` will be copied as required, and does not need to be kept alive.
     */
    capy::io_task<diagnostics> set_character_set(const character_set& charset);

    /**
     * \brief Checks whether the server is alive.
     * \details
     * If the server is alive, this function will complete without error.
     * If it's not, it will fail with the relevant network or protocol error.
     * \n
     * Note that ping requests are treated as any other type of request at the protocol
     * level, and won't be prioritized anyhow by the server. If the server is stuck
     * in a long-running query, the ping request won't be answered until the query is
     * finished.
     */
    capy::io_task<diagnostics> ping();

    /**
     * \brief Resets server-side session state, like variables and prepared statements.
     * \details
     * Resets all server-side state for the current session:
     * \n
     *   \li Rolls back any active transactions and resets autocommit mode.
     *   \li Releases all table locks.
     *   \li Drops all temporary tables.
     *   \li Resets all session system variables to their default values (including the ones set by `SET
     *       NAMES`) and clears all user-defined variables.
     *   \li Closes all prepared statements.
     * \n
     * A full reference on the affected session state can be found
     * <a href="https://dev.mysql.com/doc/c-api/8.0/en/mysql-reset-connection.html">here</a>.
     * \n
     * \n
     * This function will not reset the current physical connection and won't cause re-authentication.
     * It is faster than closing and re-opening a connection.
     * \n
     * The connection must be connected and authenticated before calling this function.
     * This function involves communication with the server, and thus may fail.
     *
     * \par Warning on character sets
     * This function will restore the connection's character set and collation **to the server's default**,
     * and not to the one specified during connection establishment. Some servers have `latin1` as their
     * default character set, which is not usually what you want. Since there is no way to know this
     * character set, \ref current_character_set will return an error after the operation succeeds.
     * We recommend always using \ref set_character_set after calling this function.
     * \n
     * You can find the character set that your server will use after the reset by running:
     * \code
     * "SELECT @@global.character_set_client, @@global.character_set_results;"
     * \endcode
     */
    capy::io_task<diagnostics> reset_connection();

    /**
     * \brief Cleanly closes the connection to the server.
     * \details
     * This function does the following:
     * \n
     * \li Sends a quit request. This is required by the MySQL protocol, to inform
     *     the server that we're closing the connection gracefully.
     * \li If the connection is using TLS (`this->uses_ssl() == true`), performs
     *     the TLS shutdown.
     * \li Closes the transport-level connection (the TCP or UNIX socket).
     * \n
     * Since this function involves writing a message to the server, it can fail.
     * Only use this function if you know that the connection is healthy and you want
     * to cleanly close it.
     * \n
     * If you don't call this function, the destructor or successive connects will
     * perform a transport-layer close. This doesn't cause any resource leaks, but may
     * cause warnings to be written to the server logs.
     */
    capy::io_task<diagnostics> close();

    /**
     * \brief Runs a set of pipelined requests.
     * \details
     * Runs the pipeline described by `req` and stores its response in `res`.
     * After the operation completes, `res` will have as many elements as stages
     * were in `req`, even if the operation fails.
     * \n
     * Request stages are seen by the server as a series of unrelated requests.
     * As a consequence, all stages are always run, even if previous stages fail.
     * \n
     * If all stages succeed, the operation completes successfully. Thus, there is no need to check
     * the per-stage error code in `res` if this operation completed successfully.
     * \n
     * If any stage fails with a non-fatal error (as per \ref is_fatal_error), the result of the operation
     * is the first encountered error. You can check which stages succeeded and which ones didn't by
     * inspecting each stage in `res`.
     * \n
     * If any stage fails with a fatal error, the result of the operation is the fatal error.
     * Successive stages will be marked as failed with the fatal error. The server may or may
     * not have processed such stages.
     */
    capy::io_task<diagnostics> run_pipeline(const pipeline_request& req, std::vector<stage_response>& res);
};

}  // namespace mysql
}  // namespace boost

#endif
