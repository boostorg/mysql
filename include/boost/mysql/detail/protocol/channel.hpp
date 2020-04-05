#ifndef MYSQL_ASIO_IMPL_CHANNEL_HPP
#define MYSQL_ASIO_IMPL_CHANNEL_HPP

#include "boost/mysql/error.hpp"
#include "boost/mysql/detail/auxiliar/bytestring.hpp"
#include "boost/mysql/detail/protocol/capabilities.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <array>
#include <optional>

namespace boost {
namespace mysql {
namespace detail {

template <typename AsyncStream>
class channel
{
	// TODO: static asserts for AsyncStream concept
	// TODO: actually we also require it to be SyncStream, name misleading
	struct ssl_block
	{
		boost::asio::ssl::context ctx;
		boost::asio::ssl::stream<AsyncStream&> stream;

		ssl_block(AsyncStream& base_stream):
			ctx(boost::asio::ssl::context::tls_client),
			stream (base_stream, ctx) {}
	};

	AsyncStream& stream_;
	std::optional<ssl_block> ssl_block_;
	std::uint8_t sequence_number_ {0};
	std::array<std::uint8_t, 4> header_buffer_ {}; // for async ops
	bytestring shared_buff_; // for async ops
	capabilities current_caps_;

	bool process_sequence_number(std::uint8_t got);
	std::uint8_t next_sequence_number() { return sequence_number_++; }

	error_code process_header_read(std::uint32_t& size_to_read); // reads from header_buffer_
	void process_header_write(std::uint32_t size_to_write); // writes to header_buffer_

	bool ssl_active() const noexcept { return ssl_block_.has_value(); }
	void create_ssl_block() { ssl_block_.emplace(stream_); }

	template <typename BufferSeq>
	std::size_t read_impl(BufferSeq&& buff, error_code& ec);

	template <typename BufferSeq>
	std::size_t write_impl(BufferSeq&& buff, error_code& ec);

	template <typename BufferSeq, typename CompletionToken>
	auto async_read_impl(BufferSeq&& buff, CompletionToken&& token);

	template <typename BufferSeq, typename CompletionToken>
	auto async_write_impl(BufferSeq&& buff, CompletionToken&& token);
public:
	channel(AsyncStream& stream): stream_(stream) {}

	template <typename Allocator>
	void read(basic_bytestring<Allocator>& buffer, error_code& code);

	void write(boost::asio::const_buffer buffer, error_code& code);

	template <typename Allocator, typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code))
	async_read(basic_bytestring<Allocator>& buffer, CompletionToken&& token);

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code))
	async_write(boost::asio::const_buffer buffer, CompletionToken&& token);

	void ssl_handshake(error_code& ec);

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code))
	async_ssl_handshake(CompletionToken&& token);

	void reset_sequence_number(std::uint8_t value = 0) { sequence_number_ = value; }
	std::uint8_t sequence_number() const { return sequence_number_; }

	using stream_type = AsyncStream;
	stream_type& next_layer() { return stream_; }

	capabilities current_capabilities() const noexcept { return current_caps_; }
	void set_current_capabilities(capabilities value) noexcept { current_caps_ = value; }

	const bytestring& shared_buffer() const noexcept { return shared_buff_; }
	bytestring& shared_buffer() noexcept { return shared_buff_; }
};

template <typename ChannelType>
using channel_stream_type = typename ChannelType::stream_type;

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/channel.hpp"

#endif
