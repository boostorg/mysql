//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_STATEMENT_HPP
#define BOOST_MYSQL_STATEMENT_HPP

#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/execute_options.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/statement_base.hpp>

#include <cassert>
#include <iterator>

namespace boost {
namespace mysql {

/**
 * \brief Represents a server-side prepared statement.
 * \details
 * You can obtain a `valid` statement by calling \ref connection::prepare_statement. You can execute
 * a statement using \ref statement::execute, and close it using \ref statement::close.
 *\n
 * Statements are proxy I/O objects, meaning that they hold a reference to the internal state of the
 * \ref connection that created them. I/O operations on a statement result in reads and writes on
 * the connection's stream. A `statement` is usable for I/O operations as long as the original \ref
 * connection is alive and open. Moving the `connection` object doesn't invalidate `statement`s
 * pointing into it, but destroying or closing it does. The executor object used by a `statement` is
 * always the underlying stream's.
 *\n
 * Statements are default-constructible and movable, but not copyable. A default constructed or
 * closed statement has `!this->valid()`. Calling any member function on an invalid
 * statement, other than assignment, results in undefined behavior.
 *\n
 * See [link mysql.prepared_statements] for more info.
 */
template <class Stream>
class statement : public statement_base
{
public:
    /**
     * \brief Default constructor.
     * \details Default constructed statements have `!this->valid()`. To obtain a valid statement,
     * call \ref connection::prepare_statement.
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
     * \brief Executes a prepared statement, passing parameters as a tuple.
     * \details
     * Starts a multi-function operation. This function will write the execute request to the
     * server and read the initial server response, but won't read the generated rows, if any. After
     * this operation completes, `result` will have \ref resultset::meta populated, and may become
     * \ref resultset::complete, if the operation did not generate any rows (e.g. it was an
     * `UPDATE`). `result` will reference the same \ref connection object that `*this` references,
     * and will be usable for server interaction as long as I/O object references to `*this` are
     * valid.
     *\n
     * If the operation generated any rows, these __must__ be read (by using any of the
     * `resultset::read_xxx` functions) before engaging in any further operation involving server
     * communication. Otherwise, the results are undefined.
     *\n
     * The statement actual parameters (`params`) are passed as a `std::tuple` of elements.
     * See the [reflink FieldLikeTuple] concept defition for more info.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     *\n
     * See [link mysql.prepared_statements this section] for more info.
     */
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void execute(
        const FieldLikeTuple& params,
        resultset<Stream>& result,
        error_code& err,
        error_info& info
    )
    {
        execute(params, execute_options(), result, err, info);
    }

    /// \copydoc execute
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void execute(const FieldLikeTuple& params, resultset<Stream>& result)
    {
        execute(params, execute_options(), result);
    }

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
        resultset<Stream>& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(
            std::forward<FieldLikeTuple>(params),
            execute_options(),
            result,
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
        resultset<Stream>& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(
            std::forward<FieldLikeTuple>(params),
            execute_options(),
            result,
            output_info,
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc execute
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void execute(
        const FieldLikeTuple& params,
        const execute_options& opts,
        resultset<Stream>& result,
        error_code& err,
        error_info& info
    );

    /// \copydoc execute
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    void execute(
        const FieldLikeTuple& params,
        const execute_options& opts,
        resultset<Stream>& result
    );

    /// \copydoc async_execute
    template <
        BOOST_MYSQL_FIELD_LIKE_TUPLE FieldLikeTuple,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type),
        class EnableIf = detail::enable_if_field_like_tuple<FieldLikeTuple>>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        FieldLikeTuple&& params,
        const execute_options& opts,
        resultset<Stream>& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(
            std::forward<FieldLikeTuple>(params),
            opts,
            result,
            get_channel().shared_info(),
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
        const execute_options& opts,
        resultset<Stream>& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief (Experimental) Executes a prepared statement, passing parameters as a range.
     * \details
     * [warning This function is experimental. Details may change in the future without notice.]
     *
     * Starts a multi-function operation. This function will write the execute request to the
     * server and read the initial server response, but won't read the generated rows, if any. After
     * this operation completes, `result` will have \ref resultset::meta populated, and may become
     * \ref resultset::complete, if the operation did not generate any rows (e.g. it was an
     * `UPDATE`). `result` will reference the same \ref connection object that `*this` references,
     * and will be usable for server interaction as long as I/O object references to `*this` are
     * valid.
     *\n
     * If the operation generated any rows, these __must__ be read (by using any of the
     * `resultset::read_xxx` functions) before engaging in any further operation involving server
     * communication. Otherwise, the results are undefined.
     *\n
     * The statement actual parameters are passed as an iterator range. There should be
     * __exactly__ as many parameters as required (as given by \ref statement::num_params).
     * Dereferencing the passed iterators should yield a type convertible to \ref field_view.
     * Both \ref field and \ref field_view satisfy this. See [reflink FieldViewFwdIterator] for
     * details.
     *\n
     * This operation involves both reads and writes on the underlying stream.
     *\n
     * See [link mysql.prepared_statements this section] for more info.
     */
    template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
    void execute(
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        const execute_options& options,
        resultset<Stream>& result,
        error_code& ec,
        error_info& info
    );

    /// \copydoc execute(FieldViewFwdIterator,FieldViewFwdIterator,const execute_options&,resultset<Stream>&,error_code&,error_info&)
    template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
    void execute(
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        const execute_options& options,
        resultset<Stream>& result
    );

    /**
     * \copydoc execute(FieldViewFwdIterator,FieldViewFwdIterator,const execute_options&,resultset<Stream>&,error_code&,error_info&)
     * \details
     * If `CompletionToken` is deferred (like `use_awaitable`), the caller must keep objects in
     * the range `\\[params_first, params_last)` alive until the  operation is initiated.
     *
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        const execute_options& options,
        resultset<Stream>& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_execute(
            params_first,
            params_last,
            options,
            result,
            get_channel().shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_execute(FieldViewFwdIterator,FieldViewFwdIterator,const execute_options&,resultset<Stream>&,CompletionToken&&)
    template <
        BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(
        FieldViewFwdIterator params_first,
        FieldViewFwdIterator params_last,
        const execute_options& options,
        resultset<Stream>& result,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Closes a prepared statement, deallocating it from the server.
     * \details
     * After this operation succeeds, `this->valid()` will return `false`, and no further functions
     * may be called on this prepared statement, other than assignment.
     *\n
     * This operation involves only writes on the underlying stream.
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
        return async_close(get_channel().shared_info(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(
        error_info& output_info,
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
