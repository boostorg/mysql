#ifndef INCLUDE_MYSQL_STREAM_HPP_
#define INCLUDE_MYSQL_STREAM_HPP_

#include <boost/asio/ip/tcp.hpp>
#include <memory>
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

class AnyPacketReader
{
	PacketHeader header_;
	int1 message_type_;
	std::unique_ptr<std::uint8_t[]> buffer_;
public:
	const PacketHeader& header() const { return header_; }
	int1 message_type() const { return message_type_; }
	ReadIterator begin() const { return buffer_.get() + 1; } // past message type, if any
	ReadIterator end() const { return buffer_.get() + header_.packet_size.value; }
	bool is_ok() const { return message_type() == mysql::ok_packet_header; }
	bool is_error() const { return message_type() == error_packet_header; }
	bool is_eof() const { return message_type() == eof_packet_header; }
	void check_error() const;

	template <typename MessageType>
	void deserialize_message(MessageType& output) const
	{
		ReadIterator last = mysql::deserialize(begin(), end(), output);
		if (last != end())
			throw std::out_of_range { "Additional data after packet end" };
		check_error();
	}

	void read(boost::asio::ip::tcp::socket& stream);
};

class AnyPacketWriter
{
	DynamicBuffer buffer_;
public:
	AnyPacketWriter(int1 sequence_number);
	DynamicBuffer& buffer() { return buffer_; }
	const DynamicBuffer& buffer() const { return buffer_; }
	void set_length();

	template <typename MessageType>
	void serialize_message(const MessageType& value)
	{
		mysql::serialize(buffer_, value);
		set_length();
	}

	void write(boost::asio::ip::tcp::socket& stream);
};

class MysqlStream
{
	boost::asio::ip::tcp::socket next_layer_; // to be converted to an async stream
	int1 sequence_number_ {0};

	void process_sequence_number(int1 got);
	void read_packet(AnyPacketReader& output);
public:
	MysqlStream(boost::asio::io_context& ctx): next_layer_ {ctx} {};
	auto& next_layer() { return next_layer_; }
	void handshake(const HandshakeParams&);

};

}



#endif /* INCLUDE_MYSQL_STREAM_HPP_ */
