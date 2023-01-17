//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_STATEMENT_HPP
#define BOOST_MYSQL_STATEMENT_HPP

#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/detail/channel/channel.hpp>

#include <cassert>
#include <iterator>

namespace boost {
namespace mysql {

/**
 * \brief Represents a server-side prepared statement.
 * \details
 * Statements are proxy I/O objects, meaning that they hold a reference to the internal state of the
 * \ref connection that created them. I/O operations on a statement result in reads and writes on
 * the connection's stream. A `statement` is usable for I/O operations as long as the \ref
 * connection that created them (or a connection move-constructed from it) is alive and open.
 * The executor object used by a `statement` is always the underlying stream's.
 *\n
 * Statements are default-constructible and movable, but not copyable. A default constructed or
 * closed statement has `!this->valid()`. Calling any member function on an invalid
 * statement, other than assignment, results in undefined behavior.
 */
template <class Stream>
class statement : public statement_base
{
public:
    /**
     * \brief Default constructor.
     * \details Default constructed statements have `this->valid() == false`.
     */
    statement() = default;

#ifndef BOOST_MYSQL_DOXYGEN
    statement& operator=(const statement&) = delete;
    statement(const statement&) = delete;
#endif

    /**
     * \brief Move constructor.
     */
    statement(statement&& other) noexcept : statement_base(other) { other.reset(); }

    /**
     * \brief Move assignment.
     */
    statement& operator=(statement&& other) noexcept
    {
        swap(other);
        return *this;
    }

    /**
     * \brief Destructor.
     * \details Doesn't invoke \ref close, as this is a network operation that may fail.
     */
    ~statement() = default;

    /// The executor type associated to this object.
    using executor_type = typename Stream::executor_type;

    /// Retrieves the executor associated to this object.
    executor_type get_executor() { return get_channel().get_executor(); }

    /**
     * \brief Executes a prepared statement.
     * \details
     * Executes the statement with the given parameters and reads the response into `result`.
     *\n
     * After this operation completes successfully, `result.has_value() == true`.
     *\n
     * The statement actual parameters (`params`) are passed as a `std::tuple` of elements.
     * See the `FieldLikeTuple` concept defition for more info. You should pass exactly as many
     * parameters as `this->num_params()`, or the operation will fail with an error.
     * String parameters should be encoded using the connection's character set.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void execute(
        const FieldLikeTuple& params,
        resultset& result,
        error_code& err,
        server_diagnostics& diag
    );

    /// \copydoc execute
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void execute(const FieldLikeTuple& params, resultset& result);

    /**
     * \copydoc execute
     * If `CompletionToken` is deferred (like `use_awaitable`), and `params` contains any reference
     * type (like `string_view`), the caller must keep the values pointed by these references alive
     * until the operation is initiated. Value types will be copied/moved as required, so don't need
     * to be kept alive.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        FieldLikeTuple&& params,
        resultset& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(
            std::forward<FieldLikeTuple>(params),
            result,
            get_channel().shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_execute
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        FieldLikeTuple&& params,
        resultset& result,
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Starts a statement execution as a multi-function operation.
     * \details
     * Writes the execute request and reads the initial server response and the column
     * metadata, but not the generated rows, if any. After this operation completes, `st` will have
     * \ref execution_state::meta populated, and may become \ref execution_state::complete
     * if the operation did not generate any rows (e.g. it was an `UPDATE`).
     *\n
     * If the operation generated any rows, these <b>must</b> be read (by using \ref
     *connection::read_one_row or \ref connection::read_some_rows) before engaging in any further
     *operation involving server communication. Otherwise, the results are undefined.
     *\n
     * The statement actual parameters (`params`) are passed as a `std::tuple` of elements.
     * See the `FieldLikeTuple` concept defition for more info. You should pass exactly as many
     * parameters as `this->num_params()`, or the operation will fail with an error.
     * String parameters should be encoded using the connection's character set.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void start_execution(
        const FieldLikeTuple& params,
        execution_state& ex,
        error_code& err,
        server_diagnostics& diag
    );

    /// \copydoc start_execution(const FieldLikeTuple&,execution_state&,error_code&,server_diagnostics&)
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void start_execution(const FieldLikeTuple& params, execution_state& st);

    /**
     * \copydoc start_execution(const FieldLikeTuple&,execution_state&,error_code&,server_diagnostics&)
     * \details
     * If `CompletionToken` is deferred (like `use_awaitable`), and `params` contains any reference
     * type (like `string_view`), the caller must keep the values pointed by these references alive
     * until the operation is initiated. Value types will be copied/moved as required, so don't need
     * to be kept alive.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_execution(
        FieldLikeTuple&& params,
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_start_execution(
            std::forward<FieldLikeTuple>(params),
            st,
            get_channel().shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_start_execution(FieldLikeTuple&&,execution_state&,CompletionToken&&)
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_execution(
        FieldLikeTuple&& params,
        execution_state& st,
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Starts a statement execution as a multi-function operation.
     * \details
     * <b>Warning: this function is experimental. Details may change in the future without
     * notice.</b>
     *\n
     * Writes the execute request and reads the initial server response and the column
     * metadata, but not the generated rows, if any. After this operation completes, `st` will have
     * \ref execution_state::meta populated, and may become \ref execution_state::complete
     * if the operation did not generate any rows (e.g. it was an `UPDATE`).
     *\n
     * If the operation generated any rows, these <b>must</b> be read (by using \ref
     * connection::read_one_row or \ref connection::read_some_rows) before engaging in any further
     * operation involving server communication. Otherwise, the results are undefined.
     *\n
     * The statement actual parameters are passed as an iterator range.
     * See the `FieldViewForwardIterator` concept defition for more info. You should pass exactly as
     * many parameters as `this->num_params()`, or the operation will fail with an error. String
     * parameters should be encoded using the connection's character set.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     */
    template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
    void start_execution(
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st,
        error_code& ec,
        server_diagnostics& diag
    );

    /// \copydoc start_execution(FieldViewFwdIterator,FieldViewFwdIterator,execution_state&,error_code&,server_diagnostics&)
    template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
    void start_execution(
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st
    );

    /**
     * \copydoc start_execution(FieldViewFwdIterator,FieldViewFwdIterator,execution_state&,error_code&,server_diagnostics&)
     * \details
     * If `CompletionToken` is deferred (like `use_awaitable`), the caller must keep objects in
     * the iterator range alive until the  operation is initiated.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_execution(
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_start_execution(
            params_first,
            params_last,
            st,
            get_channel().shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_start_execution(FieldViewFwdIterator,FieldViewFwdIterator,execution_state&,CompletionToken&&)
    template <
        BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_execution(
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        execution_state& st,
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Closes a statement, deallocating it from the server.
     * \details
     * After this operation succeeds, `this->valid()` will return `false`, and no further functions
     * may be called on this prepared statement, other than assignment.
     *\n
     * This operation involves only writes on the underlying stream.
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
        return async_close(get_channel().shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(
        server_diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

private:
    detail::channel<Stream>& get_channel() noexcept
    {
        assert(valid());
        return *static_cast<detail::channel<Stream>*>(channel_ptr());
    }
};

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/statement.hpp>

#endif
