#ifndef INCLUDE_MYSQL_STREAM_HPP_
#define INCLUDE_MYSQL_STREAM_HPP_

#include <memory>
#include <stdexcept>
#include <array>
#include <boost/beast/core/async_base.hpp> // TODO: drop this at some poin
#include <boost/asio/write.hpp> // TODO: move to impl
#include <boost/asio/yield.hpp> // TODO: move that to impl and undef it
#include "basic_types.hpp"
#include "messages.hpp"
#include "message_serialization.hpp"

namespace mysql
{


struct HandshakeParams
{
	CharacterSetLowerByte character_set;
	std::string_view username;
	std::string_view password;
	std::string_view database;
};

inline int1 get_message_type(const std::vector<std::uint8_t>& buffer, bool check_err=true);

template <typename AsyncStream>
class MysqlStream
{
	// TODO: static asserts
	AsyncStream next_layer_; // to be converted to an async stream
	int1 sequence_number_ {0};

	// TODO: this is a short-term solution for async functions.
	// header_write_buffer_ and header_read_buffer_ should be the same thing (std::array).
	// To be joined when we sanitize (de)serialization
	// read_buffer_ and write_buffer_ should be the same thing and be passed by the user
	DynamicBuffer header_write_buffer_;
	std::array<std::uint8_t, 4> header_read_buffer_ {};

	std::vector<std::uint8_t> read_buffer_;
	DynamicBuffer write_buffer_;

	void process_sequence_number(int1 got);
public:
	MysqlStream(boost::asio::io_context& ctx): next_layer_ {ctx} {};
	auto& next_layer() { return next_layer_; }
	void handshake(const HandshakeParams&);

	void read(std::vector<std::uint8_t>& buffer);

	void write(const std::vector<std::uint8_t>& buffer);

	template <typename ConstBufferSequence>
	void write(ConstBufferSequence&& buffers);

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(void))
	async_read(std::vector<std::uint8_t>& buffer, CompletionToken&& token); // TODO: generalize buffer

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(void))
	async_write(boost::asio::const_buffer buffer, CompletionToken&& token);

	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(void))
	async_handshake(const HandshakeParams& params, CompletionToken&& token);

	void reset_sequence_number() { sequence_number_ = 0; }
};

}

#include "impl/mysql_stream_impl.hpp"

#endif /* INCLUDE_MYSQL_STREAM_HPP_ */
