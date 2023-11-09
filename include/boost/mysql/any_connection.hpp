//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ANY_CONNECTION_HPP
#define BOOST_MYSQL_ANY_CONNECTION_HPP

#include <boost/mysql/buffer_params.hpp>
#include <boost/mysql/connect_params.hpp>
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
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connect_params_helpers.hpp>
#include <boost/mysql/detail/connection_impl.hpp>
#include <boost/mysql/detail/execution_concepts.hpp>
#include <boost/mysql/detail/throw_on_error_loc.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/assert.hpp>
#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {

// Forward declarations
template <class... StaticRow>
class static_execution_state;

struct any_connection_params
{
    asio::ssl::context* ssl_context{};
    std::size_t initial_read_buffer_size{buffer_params::default_initial_read_size};
};

class any_connection
{
    detail::connection_impl impl_;

public:
    any_connection(boost::asio::any_io_executor ex, any_connection_params params = {})
        : impl_(params.initial_read_buffer_size, create_stream(std::move(ex), params.ssl_context))
    {
    }

    template <
        class ExecutionContext,
        class = typename std::enable_if<std::is_constructible<
            asio::any_io_executor,
            decltype(std::declval<ExecutionContext&>().get_executor())>::value>::type>
    any_connection(ExecutionContext& ctx, any_connection_params params = {})
        : any_connection(ctx.get_executor(), params)
    {
    }

    /**
     * \brief Move constructor.
     */
    any_connection(any_connection&& other) = default;

    /**
     * \brief Move assignment.
     */
    any_connection& operator=(any_connection&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    any_connection(const any_connection&) = delete;
    any_connection& operator=(const any_connection&) = delete;
#endif

    /// The executor type associated to this object.
    using executor_type = asio::any_io_executor;

    /// Retrieves the executor associated to this object.
    executor_type get_executor() { return impl_.stream().get_executor(); }

    /// \copydoc connection::uses_ssl
    bool uses_ssl() const noexcept { return impl_.ssl_active(); }

    /// \copydoc connection::meta_mode
    metadata_mode meta_mode() const noexcept { return impl_.meta_mode(); }

    /// \copydoc connection::set_meta_mode
    void set_meta_mode(metadata_mode v) noexcept { impl_.set_meta_mode(v); }

    void connect(const connect_params& params, error_code& ec, diagnostics& diag)
    {
        impl_.connect(detail::make_view(params.server_address), detail::make_hparams(params), ec, diag);
    }

    /// \copydoc connect
    void connect(const connect_params& params)
    {
        error_code err;
        diagnostics diag;
        connect(params, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(const connect_params& params, CompletionToken&& token)
    {
        return async_connect(params, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(const connect_params& params, diagnostics& diag, CompletionToken&& token)
    {
        auto stable_prms = detail::make_stable(params);
        return impl_.async_connect(
            stable_prms.address,
            stable_prms.hparams,
            diag,
            asio::consign(std::forward<CompletionToken>(token), std::move(stable_prms.string_buffer))
        );
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(const connect_params* params, diagnostics& diag, CompletionToken&& token)
    {
        BOOST_ASSERT(params != nullptr);
        return impl_.async_connect(
            detail::make_view(params->server_address),
            detail::make_hparams(*params),
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc connection::execute
    template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_RESULTS_TYPE ResultsType>
    void execute(const ExecutionRequest& req, ResultsType& result, error_code& err, diagnostics& diag)
    {
        impl_.execute(req, result, err, diag);
    }

    /// \copydoc execute
    template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_RESULTS_TYPE ResultsType>
    void execute(const ExecutionRequest& req, ResultsType& result)
    {
        error_code err;
        diagnostics diag;
        execute(req, result, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /// \copydoc connection::async_execute
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_RESULTS_TYPE ResultsType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        ExecutionRequest&& req,
        ResultsType& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(
            std::forward<ExecutionRequest>(req),
            result,
            impl_.shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_execute
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_RESULTS_TYPE ResultsType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        ExecutionRequest&& req,
        ResultsType& result,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return impl_.async_execute(
            std::forward<ExecutionRequest>(req),
            result,
            diag,
            std::forward<CompletionToken>(token)
        );
    }

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
    void start_execution(
        const ExecutionRequest& req,
        ExecutionStateType& st,
        error_code& err,
        diagnostics& diag
    )
    {
        impl_.start_execution(req, st, err, diag);
    }

    /// \copydoc start_execution
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
    void start_execution(const ExecutionRequest& req, ExecutionStateType& st)
    {
        error_code err;
        diagnostics diag;
        start_execution(req, st, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc start_execution
     * \par Object lifetimes
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the caller is
     * responsible for managing `req`'s validity following these rules:
     * \n
     * \li If `req` is `string_view`, the string pointed to by `req`
     *     must be kept alive by the caller until the operation is initiated.
     * \li If `req` is a \ref bound_statement_tuple, and any of the parameters is a reference
     *     type (like `string_view`), the caller must keep the values pointed by these references alive
     *     until the operation is initiated.
     * \li If `req` is a \ref bound_statement_iterator_range, the caller must keep objects in
     *     the iterator range passed to \ref statement::bind alive until the  operation is initiated.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_execution(
        ExecutionRequest&& req,
        ExecutionStateType& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_start_execution(
            std::forward<ExecutionRequest>(req),
            st,
            impl_.shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_start_execution
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_execution(
        ExecutionRequest&& req,
        ExecutionStateType& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return impl_.async_start_execution(
            std::forward<ExecutionRequest>(req),
            st,
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Prepares a statement server-side.
     * \details
     * `stmt` should be encoded using the connection's character set.
     * \n
     * The returned statement has `valid() == true`.
     */
    statement prepare_statement(string_view stmt, error_code& err, diagnostics& diag)
    {
        return impl_.run(detail::prepare_statement_algo_params{&diag, stmt}, err);
    }

    /// \copydoc prepare_statement
    statement prepare_statement(string_view stmt)
    {
        error_code err;
        diagnostics diag;
        statement res = prepare_statement(stmt, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
        return res;
    }

    /**
     * \copydoc prepare_statement
     * \details
     * \par Object lifetimes
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `stmt` must be kept alive by the caller until the operation is
     * initiated.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code, boost::mysql::statement)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::statement))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, statement))
    async_prepare_statement(
        string_view stmt,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_prepare_statement(stmt, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_prepare_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::statement))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, statement))
    async_prepare_statement(
        string_view stmt,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return impl_.async_run(
            detail::prepare_statement_algo_params{&diag, stmt},
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Closes a statement, deallocating it from the server.
     * \details
     * After this operation succeeds, `stmt` must not be used again for execution.
     * \n
     * \par Preconditions
     *    `stmt.valid() == true`
     */
    void close_statement(const statement& stmt, error_code& err, diagnostics& diag)
    {
        impl_.run(impl_.make_params_close_statement(stmt, diag), err);
    }

    /// \copydoc close_statement
    void close_statement(const statement& stmt)
    {
        error_code err;
        diagnostics diag;
        close_statement(stmt, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc close_statement
     * \details
     * \par Object lifetimes
     * It is not required to keep `stmt` alive, as copies are made by the implementation as required.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close_statement(
        const statement& stmt,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_close_statement(stmt, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close_statement(
        const statement& stmt,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return impl_.async_run(
            impl_.make_params_close_statement(stmt, diag),
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Reads a batch of rows.
     * \details
     * The number of rows that will be read is unspecified. If the operation represented by `st`
     * has still rows to read, at least one will be read. If there are no more rows, or
     * `st.should_read_rows() == false`, returns an empty `rows_view`.
     * \n
     * The number of rows that will be read depends on the input buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
     * \n
     * The returned view points into memory owned by `*this`. It will be valid until
     * `*this` performs the next network operation or is destroyed.
     */
    rows_view read_some_rows(execution_state& st, error_code& err, diagnostics& diag)
    {
        return impl_.run(impl_.make_params_read_some_rows(st, diag), err);
    }

    /// \copydoc read_some_rows(execution_state&,error_code&,diagnostics&)
    rows_view read_some_rows(execution_state& st)
    {
        error_code err;
        diagnostics diag;
        rows_view res = read_some_rows(st, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
        return res;
    }

    /**
     * \copydoc read_some_rows(execution_state&,error_code&,diagnostics&)
     * \details
     * \par Handler signature
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
        return async_read_some_rows(st, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_read_some_rows(execution_state&,CompletionToken&&)
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_some_rows(
        execution_state& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return impl_.async_run(
            impl_.make_params_read_some_rows(st, diag),
            std::forward<CompletionToken>(token)
        );
    }

#ifdef BOOST_MYSQL_CXX14

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
     * The number of rows that will be read depends on the input buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
     * \n
     * Rows read by this function are owning objects, and don't hold any reference to
     * the connection's internal buffers (contrary what happens with the dynamic interface's counterpart).
     * \n
     * `SpanStaticRow` must exactly be one of the types in the `StaticRow` parameter pack.
     * The type must match the resultset that is currently being processed by `st`. For instance,
     * given `static_execution_state<T1, T2>`, when reading rows for the second resultset, `SpanStaticRow`
     * must exactly be `T2`. If this is not the case, a runtime error will be issued.
     * \n
     * This function can report schema mismatches.
     */
    template <class SpanStaticRow, class... StaticRow>
    std::size_t read_some_rows(
        static_execution_state<StaticRow...>& st,
        span<SpanStaticRow> output,
        error_code& err,
        diagnostics& diag
    )
    {
        return impl_.run(impl_.make_params_read_some_rows(st, output, diag), err);
    }

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
     * The number of rows that will be read depends on the input buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
     * \n
     * Rows read by this function are owning objects, and don't hold any reference to
     * the connection's internal buffers (contrary what happens with the dynamic interface's counterpart).
     * \n
     * `SpanStaticRow` must exactly be one of the types in the `StaticRow` parameter pack.
     * The type must match the resultset that is currently being processed by `st`. For instance,
     * given `static_execution_state<T1, T2>`, when reading rows for the second resultset, `SpanStaticRow`
     * must exactly be `T2`. If this is not the case, a runtime error will be issued.
     * \n
     * This function can report schema mismatches.
     */
    template <class SpanStaticRow, class... StaticRow>
    std::size_t read_some_rows(static_execution_state<StaticRow...>& st, span<SpanStaticRow> output)
    {
        error_code err;
        diagnostics diag;
        std::size_t res = read_some_rows(st, output, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
        return res;
    }

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
     * The number of rows that will be read depends on the input buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
     * \n
     * Rows read by this function are owning objects, and don't hold any reference to
     * the connection's internal buffers (contrary what happens with the dynamic interface's counterpart).
     * \n
     * `SpanStaticRow` must exactly be one of the types in the `StaticRow` parameter pack.
     * The type must match the resultset that is currently being processed by `st`. For instance,
     * given `static_execution_state<T1, T2>`, when reading rows for the second resultset, `SpanStaticRow`
     * must exactly be `T2`. If this is not the case, a runtime error will be issued.
     * \n
     * This function can report schema mismatches.
     *
     * \par Handler signature
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::size_t)`.
     *
     * \par Object lifetimes
     * The storage that `output` references must be kept alive until the operation completes.
     */
    template <
        class SpanStaticRow,
        class... StaticRow,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::size_t))
    async_read_some_rows(
        static_execution_state<StaticRow...>& st,
        span<SpanStaticRow> output,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_some_rows(st, output, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

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
     * The number of rows that will be read depends on the input buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
     * \n
     * Rows read by this function are owning objects, and don't hold any reference to
     * the connection's internal buffers (contrary what happens with the dynamic interface's counterpart).
     * \n
     * `SpanStaticRow` must exactly be one of the types in the `StaticRow` parameter pack.
     * The type must match the resultset that is currently being processed by `st`. For instance,
     * given `static_execution_state<T1, T2>`, when reading rows for the second resultset, `SpanStaticRow`
     * must exactly be `T2`. If this is not the case, a runtime error will be issued.
     * \n
     * This function can report schema mismatches.
     *
     * \par Handler signature
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::size_t)`.
     *
     * \par Object lifetimes
     * The storage that `output` references must be kept alive until the operation completes.
     */
    template <
        class SpanStaticRow,
        class... StaticRow,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::size_t))
    async_read_some_rows(
        static_execution_state<StaticRow...>& st,
        span<SpanStaticRow> output,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return impl_.async_run(
            impl_.make_params_read_some_rows(st, output, diag),
            std::forward<CompletionToken>(token)
        );
    }
#endif

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
    void read_resultset_head(ExecutionStateType& st, error_code& err, diagnostics& diag)
    {
        return impl_.run(impl_.make_params_read_resultset_head(st, diag), err);
    }

    /// \copydoc read_resultset_head
    template <BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
    void read_resultset_head(ExecutionStateType& st)
    {
        error_code err;
        diagnostics diag;
        read_resultset_head(st, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc read_resultset_head
     * \par Handler signature
     * The handler signature for this operation is
     * `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_resultset_head(
        ExecutionStateType& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_resultset_head(st, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_read_resultset_head
    template <
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_resultset_head(
        ExecutionStateType& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return impl_.async_run(
            impl_.make_params_read_resultset_head(st, diag),
            std::forward<CompletionToken>(token)
        );
    }

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
    void ping(error_code& err, diagnostics& diag) { impl_.run(impl_.make_params_ping(diag), err); }

    /// \copydoc ping
    void ping()
    {
        error_code err;
        diagnostics diag;
        ping(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc ping
     * \details
     * \n
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_ping(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_ping(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_ping
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_ping(diagnostics& diag, CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return impl_.async_run(impl_.make_params_ping(diag), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Resets server-side session state, like variables and prepared statements.
     * \details
     * Resets all server-side state for the current session:
     * \n
     *   \li Rolls back any active transactions and resets autocommit mode.
     *   \li Releases all table locks.
     *   \li Drops all temporary tables.
     *   \li Resets all session system variables to their default values (including the ones set by `SET
     * NAMES`) and clears all user-defined variables. \li Closes all prepared statements.
     * \n
     * A full reference on the affected session state can be found
     * <a href="https://dev.mysql.com/doc/c-api/8.0/en/mysql-reset-connection.html">here</a>.
     * \n
     * This function will not reset the current physical connection and won't cause re-authentication.
     * It is faster than closing and re-opening a connection.
     * \n
     * The connection must be connected and authenticated before calling this function.
     * This function involves communication with the server, and thus may fail.
     */
    void reset_connection(error_code& err, diagnostics& diag)
    {
        impl_.run(impl_.make_params_reset_connection(diag), err);
    }

    /// \copydoc reset_connection
    void reset_connection()
    {
        error_code err;
        diagnostics diag;
        reset_connection(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc reset_connection
     * \details
     * \n
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_reset_connection(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_reset_connection(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_reset_connection
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_reset_connection(
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return impl_.async_run(
            impl_.make_params_reset_connection(diag),
            std::forward<CompletionToken>(token)
        );
    }

    void close(error_code& err, diagnostics& diag)
    {
        this->impl_.run(this->impl_.make_params_close(diag), err);
    }

    /// \copydoc close
    void close()
    {
        error_code err;
        diagnostics diag;
        close(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(CompletionToken&& token)
    {
        return async_close(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(diagnostics& diag, CompletionToken&& token)
    {
        return this->impl_.async_run(
            this->impl_.make_params_close(diag),
            std::forward<CompletionToken>(token)
        );
    }

private:
    BOOST_MYSQL_DECL
    static std::unique_ptr<detail::any_stream> create_stream(
        asio::any_io_executor ex,
        asio::ssl::context* ctx
    );
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/any_connection.ipp>
#endif

#endif
