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

template <typename StreamType>
class resultset
{
	using channel_type = detail::channel<StreamType>;

	channel_type* channel_;
	detail::resultset_metadata meta_;
	row current_row_;
	detail::bytestring buffer_;
	detail::msgs::ok_packet ok_packet_;
	bool eof_received_ {false};
public:
	resultset(): channel_(nullptr) {};
	resultset(channel_type& channel, detail::resultset_metadata&& meta):
		channel_(&channel), meta_(std::move(meta)) {};
	resultset(channel_type& channel, detail::bytestring&& buffer, const detail::msgs::ok_packet& ok_pack):
		channel_(&channel), buffer_(std::move(buffer)), ok_packet_(ok_pack), eof_received_(true) {};

	const row* fetch_one(error_code& err);
	const row* fetch_one();
	std::vector<owning_row> fetch_many(std::size_t count, error_code& err);
	std::vector<owning_row> fetch_many(std::size_t count);
	std::vector<owning_row> fetch_all(error_code& err);
	std::vector<owning_row> fetch_all();

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, const row*))
	async_fetch_one(CompletionToken&& token);

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, std::vector<owning_row>))
	async_fetch_many(std::size_t count, CompletionToken&& token);

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, std::vector<owning_row>))
	async_fetch_all(CompletionToken&& token);

	// Is the read of the resultset complete? Pre-condition to any of the functions
	// accessing the ok_packet
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
