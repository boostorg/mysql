//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_RESULTSET_HPP
#define BOOST_MYSQL_RESULTSET_HPP

#include "boost/mysql/row.hpp"
#include "boost/mysql/metadata.hpp"
#include "boost/mysql/detail/protocol/common_messages.hpp"
#include "boost/mysql/detail/protocol/channel.hpp"
#include "boost/mysql/detail/auxiliar/bytestring.hpp"
#include "boost/mysql/detail/network_algorithms/common.hpp" // deserialize_row_fn
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <cassert>

namespace boost {
namespace mysql {

/**
 * \brief Represents tabular data retrieved from the MySQL server.
 *        See [link mysql.resultsets this section] for more info.
 * \details Returned as the result of a [link mysql.queries text query]
 * or a [link mysql.prepared_statements statement execution]. It is an I/O object
 * that allows reading rows progressively. [link mysql.resultsets This section]
 * provides an in-depth explanation of the mechanics of this class.
 *
 * Resultsets are default-constructible and movable, but not copyable. 
 * [refmem resultset valid] returns `false` for default-constructed 
 * and moved-from resultsets. Calling any member function on an invalid
 * resultset, other than assignment, results in undefined behavior.
 */
template <
    class Stream
>
class resultset
{
    detail::deserialize_row_fn deserializer_ {};
    detail::channel_observer_ptr<Stream> channel_;
    detail::resultset_metadata meta_;
    detail::bytestring ok_packet_buffer_;
    detail::ok_packet ok_packet_;
    bool eof_received_ {false};

    error_info& shared_info() noexcept { assert(channel_); return channel_->shared_info(); }

    struct read_one_op;
    struct read_many_op;
    struct read_many_op_impl;

  public:
    /// \brief Default constructor.
    /// \details Default constructed resultsets have [refmem resultset valid] return `false`.
    resultset(): channel_(nullptr) {};

    /**
      * \brief Move constructor.
      * \details The constructed resultset will be valid if `other` is valid.
      * After this operation, `other` is guaranteed to be invalid.
      */
    resultset(resultset&& other) = default;

    /**
      * \brief Move assignment.
      * \details The assigned-to resultset will be valid if `other` is valid.
      */
    resultset& operator=(resultset&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    resultset(const resultset&) = delete;
    resultset& operator=(const resultset&) = delete;

    // Private, do not use
    resultset(detail::channel<Stream>& channel, detail::resultset_metadata&& meta,
        detail::deserialize_row_fn deserializer):
        deserializer_(deserializer), channel_(&channel), meta_(std::move(meta)) {};
    resultset(detail::channel<Stream>& channel, detail::bytestring&& buffer,
        const detail::ok_packet& ok_pack):
        channel_(&channel), ok_packet_buffer_(std::move(buffer)), ok_packet_(ok_pack), eof_received_(true) {};
#endif

    /// The executor type associated to the object.
    using executor_type = typename detail::channel<Stream>::executor_type;

    /// Retrieves the executor associated to the object.
    executor_type get_executor() { assert(channel_); return channel_->get_executor(); }

    /// Retrieves the stream object associated with the underlying connection.
    Stream& next_layer() noexcept { assert(channel_); return channel_->next_layer(); }

    /// Retrieves the stream object associated with the underlying connection.
    const Stream& next_layer() const noexcept { assert(channel_); return channel_->next_layer(); }

    /**
     * \brief Reads a single row (sync with error code version).
     * \details Returns `true` if a row was read successfully, `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset always returns `false`.
     * 
     * If the operation succeeds and returns `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but returns `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     */
    bool read_one(row& output, error_code& err, error_info& info);

    /**
     * \brief Reads a single row (sync with exceptions version).
     * \details Returns `true` if a row was read successfully, `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset always returns `false`.
     * 
     * If the operation succeeds and returns `true`, the new row will be
     * read against `output`, possibly reusing its memory. If the operation
     * succeeds but returns `false`, `output` will be set to the empty row
     * (as if [refmem row clear] was called). If the operation fails,
     * `output` is left in a valid but undetrmined state.
     */
    bool read_one(row& output);

    /**
     * \brief Reads a single row (async without [reflink error_info] version).
     * \details Completes with `true` if a row was read successfully, and with `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset always returns `false`.
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
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, bool))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, bool))
    async_read_one(row& output, CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_read_one(output, shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads a single row (async with [reflink error_info] version).
     * \details Completes with `true` if a row was read successfully, and with `false` if
     * there was an error or there were no more rows to read. Calling
     * this function on a complete resultset always returns `false`.
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
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, bool))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, bool))
    async_read_one(
    	row& output,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /// Reads several rows, up to a maximum (sync with error code version).
    std::vector<row> read_many(std::size_t count, error_code& err, error_info& info);

    /// Reads several rows, up to a maximum (sync with exceptions version).
    std::vector<row> read_many(std::size_t count);

    /**
     * \brief Reads several rows, up to a maximum
     *        (async without [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, std::vector<row>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::vector<row>))
    async_read_many(
        std::size_t count,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return async_read_many(count, shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads several rows, up to a maximum
     *        (async with [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, std::vector<row>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::vector<row>))
    async_read_many(
        std::size_t count,
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /// Reads all available rows (sync with error code version).
    std::vector<row> read_all(error_code& err, error_info& info);

    /// Reads all available rows (sync with exceptions version).
    std::vector<row> read_all();

    /**
     * \brief Reads all available rows (async without [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, std::vector<row>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::vector<row>))
    async_read_all(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_read_all(shared_info(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads all available rows (async with [reflink error_info] version).
     * \details
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::vector<boost::mysql::row>)`.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, std::vector<row>))
        CompletionToken
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::vector<row>))
    async_read_all(
        error_info& output_info,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    );

    /**
     * \brief Returns whether this object represents a valid resultset.
     * \details Returns `false` for default-constructed and moved-from resultsets.
     * Calling any member function on an invalid resultset,
     * other than assignment, results in undefined behavior.
     */
    bool valid() const noexcept { return channel_ != nullptr; }

    /// \brief Returns whether the resultset has been completely read or not.
    /// \details See [link mysql.resultsets.complete this section] for more info.
    bool complete() const noexcept { return eof_received_; }

    /**
     * \brief Returns [link mysql.resultsets.metadata metadata] about the fields in the query.
     * \details There will be as many [reflink field_metadata] objects as fields
     * in the SQL query, and in the same order.
     */
    const std::vector<field_metadata>& fields() const noexcept { return meta_.fields(); }

    /**
     * \brief The number of rows affected by the SQL that generated this resultset.
     * \details The resultset __must be [link mysql.resultsets.complete complete]__
     * before calling this function.
     */
    std::uint64_t affected_rows() const noexcept { assert(complete()); return ok_packet_.affected_rows.value; }

    /**
     * \brief The last insert ID produced by the SQL that generated this resultset.
     * \details The resultset __must be [link mysql.resultsets.complete complete]__
     * before calling this function.
     */
    std::uint64_t last_insert_id() const noexcept { assert(complete()); return ok_packet_.last_insert_id.value; }

    /**
     * \brief The number of warnings produced by the SQL that generated this resultset.
     * \details The resultset __must be [link mysql.resultsets.complete complete]__
     *  before calling this function.
     */
    unsigned warning_count() const noexcept { assert(complete()); return ok_packet_.warnings; }

    /**
     * \brief Additionat text information about the execution of
     *        the SQL that generated this resultset.
     * \details The resultset __must be [link mysql.resultsets.complete complete]__
     * before calling this function. The returned string is guaranteed to be valid
     * until the resultset object is destroyed.
     */
    boost::string_view info() const noexcept { assert(complete()); return ok_packet_.info.value; }

    /// Rebinds the resultset type to another executor.
    template <class Executor>
    struct rebind_executor
    {
        /// The resultset type when rebound to the specified executor.
        using other = resultset<
            typename Stream:: template rebind_executor<Executor>::other
        >;
    };
};

/// A resultset associated with a [reflink tcp_connection].
using tcp_resultset = resultset<boost::asio::ip::tcp::socket>;

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(BOOST_MYSQL_DOXYGEN)

/// A resultset associated with a [reflink unix_connection].
using unix_resultset = resultset<boost::asio::local::stream_protocol::socket>;

#endif

} // mysql
} // boost

#include "boost/mysql/impl/resultset.hpp"

#endif
