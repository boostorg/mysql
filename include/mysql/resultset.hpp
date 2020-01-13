#ifndef MYSQL_ASIO_RESULTSET_HPP
#define MYSQL_ASIO_RESULTSET_HPP

#include "mysql/row.hpp"
#include "mysql/metadata.hpp"
#include "mysql/impl/messages.hpp"
#include "mysql/impl/channel.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <cassert>

namespace mysql
{

/**
 * \brief Represents tabular data retrieved from the MySQL server.
 * \details Returned as the result of a query (\see connection::query).
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

	channel_type* channel_;
	detail::resultset_metadata meta_;
	row current_row_;
	detail::bytestring buffer_;
	detail::ok_packet ok_packet_;
	bool eof_received_ {false};
public:
	/// Default constructor.
	resultset(): channel_(nullptr) {};

	// Private, do not use
	resultset(channel_type& channel, detail::resultset_metadata&& meta):
		channel_(&channel), meta_(std::move(meta)) {};
	resultset(channel_type& channel, detail::bytestring&& buffer, const detail::ok_packet& ok_pack):
		channel_(&channel), buffer_(std::move(buffer)), ok_packet_(ok_pack), eof_received_(true) {};

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
	const row* fetch_one(error_code& err);

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
	std::vector<owning_row> fetch_many(std::size_t count, error_code& err);

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
	std::vector<owning_row> fetch_all(error_code& err);

	/// Fetches all available rows (sync with exceptions version).
	std::vector<owning_row> fetch_all();

	/// Fetchs a single row (async version).
	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, const row*))
	async_fetch_one(CompletionToken&& token);

	/// Fetches at most count rows (async version).
	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, std::vector<owning_row>))
	async_fetch_many(std::size_t count, CompletionToken&& token);

	/// Fetches all available rows (async version).
	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, std::vector<owning_row>))
	async_fetch_all(CompletionToken&& token);

	bool valid() const noexcept { return channel_ != nullptr; }
	bool complete() const noexcept { return eof_received_; }
	const std::vector<field_metadata>& fields() const noexcept { return meta_.fields(); }
	std::uint64_t affected_rows() const noexcept { assert(complete()); return ok_packet_.affected_rows.value; }
	std::uint64_t last_insert_id() const noexcept { assert(complete()); return ok_packet_.last_insert_id.value; }
	unsigned warning_count() const noexcept { assert(complete()); return ok_packet_.warnings.value; }
	std::string_view info() const noexcept { assert(complete()); return ok_packet_.info.value; }
	// TODO: status flags accessors
};

using tcp_resultset = resultset<boost::asio::ip::tcp::socket>;

}

#include "mysql/impl/resultset.hpp"

#endif
