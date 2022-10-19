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

#include <boost/asio/async_result.hpp>

#include <cassert>

namespace boost {
namespace mysql {

template <class Stream>
class resultset : public resultset_base
{
public:
    resultset() = default;
    resultset(const resultset&) = delete;
    resultset(resultset&& other) noexcept : resultset_base(std::move(other)) { other.reset(); }
    resultset& operator=(const resultset&) = delete;
    resultset& operator=(resultset&& rhs) noexcept
    {
        swap(rhs);
        rhs.reset();
        return *this;
    }
    ~resultset() = default;

    using executor_type = typename Stream::executor_type;

    executor_type get_executor() { return get_channel().get_executor(); }

    /**
     * \brief Reads a single row (sync with error code version).
     * \details Returns `true` if a row was read successfully, `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset_base always returns `false`.
     *
     * If the operation succeeds and returns `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but returns `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     */
    row_view read_one(error_code& err, error_info& info);

    /**
     * \brief Reads a single row (sync with exceptions version).
     * \details Returns `true` if a row was read successfully, `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset_base always returns `false`.
     *
     * If the operation succeeds and returns `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but returns `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     */
    row_view read_one();

    /**
     * \brief Reads a single row (async without [reflink error_info] version).
     * \details Completes with `true` if a row was read successfully, and with `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset_base always returns `false`.
     *
     * If the operation succeeds and completes with `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but completes with `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, bool)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::row_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, row_view))
    async_read_one(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_read_one(get_channel().shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads a single row (async with [reflink error_info] version).
     * \details Completes with `true` if a row was read successfully, and with `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset_base always returns `false`.
     *
     * If the operation succeeds and completes with `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but completes with `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, bool)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::row_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, row_view))
    async_read_one(
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Reads a single row (sync with error code version).
     * \details Returns `true` if a row was read successfully, `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset_base always returns `false`.
     *
     * If the operation succeeds and returns `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but returns `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     */
    void read_one(row& output, error_code& err, error_info& info);

    /**
     * \brief Reads a single row (sync with exceptions version).
     * \details Returns `true` if a row was read successfully, `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset_base always returns `false`.
     *
     * If the operation succeeds and returns `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but returns `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     */
    void read_one(row& output);

    /**
     * \brief Reads a single row (async without [reflink error_info] version).
     * \details Completes with `true` if a row was read successfully, and with `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset_base always returns `false`.
     *
     * If the operation succeeds and completes with `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but completes with `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, bool)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
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

    /**
     * \brief Reads a single row (async with [reflink error_info] version).
     * \details Completes with `true` if a row was read successfully, and with `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset_base always returns `false`.
     *
     * If the operation succeeds and completes with `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but completes with `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     *
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, bool)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_one(
        row& output,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    rows_view read_some(error_code& err, error_info& info);
    rows_view read_some();

    /**
     * \brief Reads several rows, up to a maximum
     *        (async without [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_some(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_read_some(get_channel().shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads several rows, up to a maximum
     *        (async with [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_some(
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    void read_some(rows& output, error_code& err, error_info& info);
    void read_some(rows& output);

    /**
     * \brief Reads several rows, up to a maximum
     *        (async without [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
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

    /**
     * \brief Reads several rows, up to a maximum
     *        (async with [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_some(
        rows& output,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /// Reads all available rows (sync with error code version).
    rows_view read_all(error_code& err, error_info& info);

    /// Reads all available rows (sync with exceptions version).
    rows_view read_all();

    /**
     * \brief Reads all available rows (async without [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_all(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_read_all(get_channel().shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads all available rows (async with [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, rows_view))
    async_read_all(
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /// Reads all available rows (sync with error code version).
    void read_all(rows& output, error_code& err, error_info& info);

    /// Reads all available rows (sync with exceptions version).
    void read_all(rows& output);

    /**
     * \brief Reads all available rows (async without [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
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

    /**
     * \brief Reads all available rows (async with [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_read_all(
        rows& output,
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
