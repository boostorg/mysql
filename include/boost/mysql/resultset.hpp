//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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

/**
 * \defgroup resultsets Resultsets
 * \brief Classes and functions related to resultsets.
 */

namespace boost {
namespace mysql {

/**
 * \ingroup resultsets
 * \brief Represents tabular data retrieved from the MySQL server.
 * \details Returned as the result of a query (see connection::query,
 * prepared_statement::execute).
 *
 * A resultset does not read all the retrieved information into memory
 * directly. Instead, you use fetch_one, fetch_many
 * or fetch_all to load rows progressively. This allows for better efficiency.
 *
 * A resultset can be in two states:
 * - Complete, meaning that all data has already been retrieved for this
 *   resultset.
 * - Not complete, meaning that there is still rows to be read.
 *
 * You can test whether a resultset is complete or not by calling
 * resultset::complete().
 *
 * Resultsets also contain metadata about the fields in the query.
 * You can access them at any point using resultset::fields().
 *
 * The result of a SQL statement that does not return any data (e.g.
 * an UPDATE statement) is an empty resultset. An empty resultset
 * is complete since the very beginning, and fields() will be empty.
 *
 * Resultsets contain additional data about the execution of the query,
 * like the warning count or the last insert ID. You may access this information
 * **only after the resultset is complete**. Failing to do so results
 * in undefined behavior.
 *
 * Resultsets are default-constructible. A default-constructed resultset has
 * valid() == false. It is undefined to call any member function on an invalid
 * resultset, other than assignment. Resultsets are movable but not copyable.
 */
template <
    typename StreamType
>
class resultset
{
    using channel_type = detail::channel<StreamType>;

    detail::deserialize_row_fn deserializer_ {};
    channel_type* channel_;
    detail::resultset_metadata meta_;
    row current_row_;
    detail::bytestring buffer_;
    detail::ok_packet ok_packet_;
    bool eof_received_ {false};

    struct fetch_one_op;
    struct fetch_many_op;
    struct fetch_many_op_impl;

  public:
    /// Default constructor.
    resultset(): channel_(nullptr) {};

    // Private, do not use
    resultset(channel_type& channel, detail::resultset_metadata&& meta, detail::deserialize_row_fn deserializer):
        deserializer_(deserializer), channel_(&channel), meta_(std::move(meta)) {};
    resultset(channel_type& channel, detail::bytestring&& buffer, const detail::ok_packet& ok_pack):
        channel_(&channel), buffer_(std::move(buffer)), ok_packet_(ok_pack), eof_received_(true) {};

    /// The executor type associated to the object.
    using executor_type = typename channel_type::executor_type;

    /// Retrieves the executor associated to the object.
    executor_type get_executor() { assert(channel_); return channel_->get_executor(); }

    /// Retrieves the stream object associated with the underlying connection.
    StreamType& next_layer() noexcept { assert(channel_); return channel_->next_layer(); }

    /// Retrieves the stream object associated with the underlying connection.
    const StreamType& next_layer() const noexcept { assert(channel_); return channel_->next_layer(); }

    /**
     * \brief Fetches a single row (sync with error code version).
     * \details The returned object will be nullptr if there are no more rows
     * to be read. Calling fetch_one on a complete resultset returns nullptr.
     *
     * The returned row points into memory owned by the resultset. Destroying
     * or moving the resultset object invalidates the returned row. Calling
     * any of the fetch methods again does also invalidate the returned row.
     * fetch_one is the fetch method that performs the less memory allocations
     * of the three.
     */
    const row* fetch_one(error_code& err, error_info& info);

    /// Fetches a single row (sync with exceptions version).
    const row* fetch_one();

    /**
     * \brief Fetches at most count rows (sync with error code version).
     * \details The returned rows are guaranteed to be valid as long as the
     * resultset is alive. Contrary to fetch_one, subsequent calls to any of
     * the fetch methods do not invalidate the returned rows.
     *
     * Only if count is **greater** than the available number of rows,
     * the resultset will be completed.
     */
    std::vector<owning_row> fetch_many(std::size_t count, error_code& err, error_info& info);

    /// Fetches at most count rows (sync with exceptions version).
    std::vector<owning_row> fetch_many(std::size_t count);

    /**
     * \brief Fetches all available rows (sync with error code version).
     * \details The returned rows are guaranteed to be valid as long as the
     * resultset is alive. Contrary to fetch_one, subsequent calls to any of
     * the fetch methods do not invalidate the returned rows.
     *
     * The resultset is guaranteed to be complete() after this call returns.
     */
    std::vector<owning_row> fetch_all(error_code& err, error_info& info);

    /// Fetches all available rows (sync with exceptions version).
    std::vector<owning_row> fetch_all();

    /// Handler signature for resultset::async_fetch_one.
    using fetch_one_signature = void(error_code, const row*);

    /// Fetchs a single row (async version).
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(fetch_one_signature) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, fetch_one_signature)
    async_fetch_one(CompletionToken&& token, error_info* info=nullptr);

    /// Handler signature for resultset::async_fetch_many.
    using fetch_many_signature = void(error_code, std::vector<owning_row>);

    /// Fetches at most count rows (async version).
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(fetch_many_signature) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, fetch_many_signature)
    async_fetch_many(std::size_t count, CompletionToken&& token, error_info* info=nullptr);

    /// Handler signature for resultset::async_fetch_all.
    using fetch_all_signature = void(error_code, std::vector<owning_row>);

    /// Fetches all available rows (async version).
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(fetch_all_signature) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, fetch_all_signature)
    async_fetch_all(CompletionToken&& token, error_info* info=nullptr);

    /**
     * \brief Returns whether this object represents a valid resultset.
     * \details Returns false for default-constructed resultsets. It is
     * undefined to call any member function on an invalid resultset,
     * except assignment.
     */
    bool valid() const noexcept { return channel_ != nullptr; }

    /// Returns whether the resultset has been completely read or not.
    bool complete() const noexcept { return eof_received_; }

    /**
     * \brief Returns metadata about the fields in the query.
     * \details There will be as many field_metadata objects as fields
     * in the SQL query, and in the same order. For SQL statements
     * that do not return values (like UPDATEs), it will be empty.
     * \see field_metadata for more details.
     */
    const std::vector<field_metadata>& fields() const noexcept { return meta_.fields(); }

    /**
     * \brief The number of rows affected by the SQL that generated this resultset.
     * \warning The resultset **must be complete** before calling this function.
     */
    std::uint64_t affected_rows() const noexcept { assert(complete()); return ok_packet_.affected_rows.value; }

    /**
     * \brief The last insert ID produced by the SQL that generated this resultset.
     * \warning The resultset **must be complete** before calling this function.
     */
    std::uint64_t last_insert_id() const noexcept { assert(complete()); return ok_packet_.last_insert_id.value; }

    /**
     * \brief The number of warnings produced by the SQL that generated this resultset.
     * \warning The resultset **must be complete** before calling this function.
     */
    unsigned warning_count() const noexcept { assert(complete()); return ok_packet_.warnings.value; }

    /**
     * \brief Additionat text information about the execution of
     *        the SQL that generated this resultset.
     * \warning The resultset **must be complete** before calling this function.
     */
    std::string_view info() const noexcept { assert(complete()); return ok_packet_.info.value; }
};

/**
 * \ingroup resultsets
 * \brief Specialization of a resultset associated with a boost::mysql::tcp_connection.
 */
using tcp_resultset = resultset<boost::asio::ip::tcp::socket>;

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(BOOST_MYSQL_DOXYGEN)

/**
 * \ingroup resultsets
 * \brief Specialization of a resultset associated with a boost::mysql::unix_connection.
 */
using unix_resultset = resultset<boost::asio::local::stream_protocol::socket>;

#endif

} // mysql
} // boost

#include "boost/mysql/impl/resultset.hpp"

#endif
