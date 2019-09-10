#ifndef INCLUDE_MYSQL_STREAM_HPP_
#define INCLUDE_MYSQL_STREAM_HPP_

#include <memory>
#include <stdexcept>
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

	void process_sequence_number(int1 got);
public:
	MysqlStream(boost::asio::io_context& ctx): next_layer_ {ctx} {};
	auto& next_layer() { return next_layer_; }
	void handshake(const HandshakeParams&);

	void read(std::vector<std::uint8_t>& buffer);

	void write(const std::vector<std::uint8_t>& buffer);

	template <typename ConstBufferSequence>
	void write(ConstBufferSequence&& buffers);

	void reset_sequence_number() { sequence_number_ = 0; }
};

}

#include "impl/mysql_stream_impl.hpp"

#endif /* INCLUDE_MYSQL_STREAM_HPP_ */
