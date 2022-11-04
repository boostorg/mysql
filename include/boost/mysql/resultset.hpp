//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_RESULTSET_HPP
#define BOOST_MYSQL_RESULTSET_HPP

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/use_views.hpp>

#include <boost/asio/async_result.hpp>

#include <cassert>

namespace boost {
namespace mysql {

/**
 * \brief Holds state and data about a query or statement execution, and allows reading the produced
 *        rows.
 * \details
 * Resultsets are obtained as a result of a SQL statement execution, by calling \ref
 * connection::query or \ref statement::execute. A resultset holds metadata about the operation
 * (\ref meta), allows you to read the resulting rows ( \ref read_one, \ref read_some, \ref
 * read_all), and stores additional information about the execution ("EOF packet data", in MySQL
 * slang: \ref affected_rows, \ref last_insert_id, and so on). Resultsets are the glue that join
 * the different pieces of \ref connection::query and \ref statement::execute multi-function
 * operations.
 * \n
 * Resultsets are proxy I/O objects, meaning that they hold a reference to the internal state of the
 * \ref connection that created them. I/O operations on a resultset result in reads and writes on
 * the connection's stream. A `resultset` is usable for I/O operations as long as the original \ref
 * connection is alive and open. Moving the `connection` object doesn't invalidate `resultset`s
 * pointing into it, but destroying or closing it does. The executor object used by a `resultset` is
 * always the underlying stream's.
 * \n
 * Resultsets are default-constructible and movable, but not copyable. A default constructed or
 * closed resultset has `!this->valid()`. Calling any member function on an invalid
 * resultset, other than assignment, results in undefined behavior.
 * \n
 * See [link mysql.resultsets this section] for more info.
 */
template <class Stream>
class resultset : public resultset_base
{
public:
    /**
     * \brief Default constructor.
     * \details Default constructed resultsets have `!this->valid()`.
     */
    resultset() = default;

#ifndef BOOST_MYSQL_DOXYGEN
    resultset(const resultset&) = delete;
    resultset& operator=(const resultset&) = delete;
#endif

    /**
     * \brief Move constructor.
     * \details Views obtained from `other.meta()` and `other.info()` remain valid.
     */
    resultset(resultset&& other) noexcept : resultset_base(std::move(other)) { other.reset(); }

    /**
     * \brief Move assignment.
     * \details Views obtained from `other.meta()` and `other.info()` remain valid. Views obtained
     * from `this->meta()` and `this->info()` are invalidated.
     */
    resultset& operator=(resultset&& other) noexcept
    {
        swap(other);
        other.reset();
        return *this;
    }

    /**
     * \brief Destructor.
     * \details Views obtained from `this->meta()` and `this->info()` are invalidated.
     */
    ~resultset() = default;

    /// The executor type associated to this object.
    using executor_type = typename Stream::executor_type;

    /// Retrieves the executor associated to this object.
    executor_type get_executor() { return get_channel().get_executor(); }

    /**
     * \brief Reads a single row.
     * \details
     * Returns `true` if a row was read successfully. If there were no more rows to read,
     * it returns `false` and sets `output` to an empty row.
     *\n
     * Use this operation to read large resultsets that may not entirely fit in memory, or when
     * individual row processing is preferred.
     */
    bool read_one(row& output, error_code& err, error_info& info);

    /// \copydoc read_one(row&,error_code&,error_info&)
    bool read_one(row& output);

    /**
     * \copydoc read_one(row&,error_code&,error_info&)
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, bool)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, bool))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, bool))
    async_read_one(
        row& output,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_one(
            output,
            get_channel().shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_read_one(row&,CompletionToken&&)
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, bool))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, bool))
    async_read_one(
        row& output,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Reads a single row as a \ref row_view.
     * \details
     * If a row was read successfully, returns a non-empty \ref row_view.
     * If there were no more rows to read, returns an empty `row_view`.
     *\n
     * The returned view points into the \ref connection that `*this` references.
     * It will be valid until the `connection` performs any other read operation
     * or is destroyed.
     */
    row_view read_one(use_views_t, error_code& err, error_info& info);

    /// \copydoc read_one(use_views_t,error_code&,error_info&)
    row_view read_one(use_views_t);

    /**
     * \copydoc read_one(use_views_t,error_code&,error_info&)
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::row_view)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::row_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, row_view))
    async_read_one(
        use_views_t,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_one(
            use_views,
            get_channel().shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_read_one(use_views_t,CompletionToken&&)
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::row_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, row_view))
    async_read_one(
        use_views_t,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Reads a batch of rows.
     * \details
     * The number of rows that will be read is unspecified. The contents of `output` will be
     * replaced by the read rows, so you can obtain the number of read rows using `output.size()`.
     * If this resultset has still rows to read, at least one will be read. If there are no more
     * rows to be read, `output` will be `empty()`.
     * \n
     * The number of rows that will be read depends on the input buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in \ref
     * connection::connect, using \ref buffer_params::initial_read_buffer_size. The buffer may be
     * grown bigger if required by other read operations.
     * \n
     * This is the most complex but most performant way of reading rows. You may consider
     * the overload returning a \ref rows_view object, too, which performs less copying.
     */
    void read_some(rows& output, error_code& err, error_info& info);

    /// \copydoc read_some(rows&,error_code&,error_info&)
    void read_some(rows& output);

    /**
     * \copydoc read_some(rows&,error_code&,error_info&)
     * \details
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_some(
        rows& output,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_some(
            output,
            get_channel().shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_read_some(rows&,CompletionToken&&)
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_some(
        rows& output,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Reads a batch of rows as a \ref rows_view.
     * \details
     * The number of rows that will be read is unspecified. The contents of `output` will be
     * replaced by the read rows, so you can obtain the number of read rows using `output.size()`.
     * If this resultset has still rows to read, at least one will be read. If there are no more
     * rows to be read, `output` will be `empty()`.
     * \n
     * The number of rows that will be read depends on the input buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in \ref
     * connection::connect, using \ref buffer_params::initial_read_buffer_size. The buffer may be
     * grown bigger if required by other read operations.
     * \n
     * The returned view points into the \ref connection that `*this` references.
     * It will be valid until the `connection` performs any other read operation
     * or is destroyed.
     * \n
     * This is the most complex but most performant way of reading rows.
     */
    rows_view read_some(use_views_t, error_code& err, error_info& info);

    /// \copydoc read_some(use_views_t,error_code&,error_info&)
    rows_view read_some(use_views_t);

    /**
     * \copydoc read_some(use_views_t,error_code&,error_info&)
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::rows_view)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_some(
        use_views_t,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_some(
            use_views,
            get_channel().shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_read_some(use_views_t,CompletionToken&&)
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_some(
        use_views_t,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Reads all remaining rows.
     * \details
     * The contents of `output` will be replaced by the read rows, so you can obtain the number of
     * read rows using `output.size()`. After this operation succeeds, `this->complete() == true`.
     * \n
     * This function requires fitting all the rows in the resultset into memory. It is a good choice
     * for small resultsets.
     */
    void read_all(rows& output, error_code& err, error_info& info);

    /// \copydoc read_all(rows&,error_code&,error_info&)
    void read_all(rows& output);

    /**
     * \copydoc read_all(rows&,error_code&,error_info&)
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_all(
        rows& output,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_all(
            output,
            get_channel().shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_read_all(rows&,CompletionToken&&)
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_all(
        rows& output,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Reads all remaining rows.
     * \details
     * The contents of `output` will be replaced by the read rows, so you can obtain the number of
     * read rows using `output.size()`. After this operation succeeds, `this->complete() == true`.
     * \n
     * This function requires fitting all the rows in the resultset into memory. It is a good choice
     * for small resultsets.
     * \n
     * The returned view points into the \ref connection that `*this` references.
     * It will be valid until the `connection` performs any other read operation
     * or is destroyed.
     */
    rows_view read_all(use_views_t, error_code& err, error_info& info);

    /// \copydoc read_all(use_views_t,error_code&,error_info&)
    rows_view read_all(use_views_t);

    /**
     * \copydoc read_all(use_views_t,error_code&,error_info&)
     * \details
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_all(
        use_views_t,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_all(
            use_views,
            get_channel().shared_info(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_read_all(use_views_t,CompletionToken&&)
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_all(
        use_views_t,
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

#include <boost/mysql/impl/resultset.hpp>

#endif
