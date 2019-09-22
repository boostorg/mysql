#ifndef INCLUDE_IMPL_MYSQL_STREAM_IMPL_HPP_
#define INCLUDE_IMPL_MYSQL_STREAM_IMPL_HPP_

#include "message_serialization.hpp"
#include "auth.hpp"
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <iostream>
#include <cassert>
#include <sstream>

namespace mysql
{
namespace detail
{

template <typename T1, typename... Flags>
constexpr bool all_set(T1 input, Flags... flags)
{
	return ((input & flags) && ...);
}

inline void check_capabilities(mysql::int4 server_capabilities)
{
	bool ok = all_set(server_capabilities,
		mysql::CLIENT_CONNECT_WITH_DB,
		mysql::CLIENT_PROTOCOL_41,
		mysql::CLIENT_PLUGIN_AUTH,
		mysql::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA,
		mysql::CLIENT_DEPRECATE_EOF
	);
	if (!ok)
		throw std::runtime_error { "Missing server capabilities, server not supported" };
}

inline void check_authentication_method(const mysql::Handshake& handshake)
{
	if (handshake.auth_plugin_name.value != "mysql_native_password")
		throw std::runtime_error { "Unsupported authentication method" }; // TODO: we should be responding with our method
	if (handshake.auth_plugin_data.size() != mysql::mysql_native_password::challenge_length)
		throw std::runtime_error { "Bad authentication data length" };
}

constexpr mysql::int4 BASIC_CAPABILITIES_FLAGS =
		mysql::CLIENT_PROTOCOL_41 |
		mysql::CLIENT_PLUGIN_AUTH |
		mysql::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
		mysql::CLIENT_DEPRECATE_EOF |
		mysql::CLIENT_CONNECT_WITH_DB;

}
}


inline mysql::int1 mysql::get_message_type(const std::vector<std::uint8_t>& buffer, bool check_err)
{
	mysql::int1 res;
	ReadIterator current = mysql::deserialize(buffer, res);
	if (check_err && res == error_packet_header)
	{
		ErrPacket error_packet;
		deserialize(current, buffer.data() + buffer.size(), error_packet);
		std::ostringstream ss;
		ss << "SQL error: " << error_packet.error_message.value
		   << " (" << error_packet.error_code << ")";
		throw std::runtime_error { ss.str() };
	}
	return res;
}

template <typename AsyncStream>
void mysql::MysqlStream<AsyncStream>::process_sequence_number(int1 got)
{
	if (got != sequence_number_++)
		throw std::runtime_error { "Mismatched sequence number" };
}

template <typename AsyncStream>
void mysql::MysqlStream<AsyncStream>::read(std::vector<std::uint8_t>& buffer)
{
	PacketHeader header;
	uint8_t header_buffer [4];
	std::size_t current_size = 0;
	do
	{
		boost::asio::read(next_layer_, boost::asio::buffer(header_buffer));
		deserialize(std::begin(header_buffer), std::end(header_buffer), header);
		process_sequence_number(header.sequence_number);
		buffer.resize(current_size + header.packet_size.value);
		boost::asio::read(next_layer_, boost::asio::buffer(buffer.data() + current_size, header.packet_size.value));
		current_size += header.packet_size.value;
	} while (header.packet_size.value == 0xffffff);
}

template <typename AsyncStream>
void mysql::MysqlStream<AsyncStream>::write(const std::vector<std::uint8_t>& buffer)
{
	write(boost::asio::buffer(buffer.data(), buffer.size()));
}

template <typename AsyncStream>
template <typename ConstBufferSequence>
void mysql::MysqlStream<AsyncStream>::write(ConstBufferSequence&& buffers)
{
	PacketHeader header;
	DynamicBuffer header_buffer; // TODO: change to a plain uint8_t when we generalize serialization
	std::size_t current_size = 0;
	constexpr std::size_t MAX_PACKET_SIZE = 0xffffff;

	auto first = boost::asio::buffer_sequence_begin(buffers);
	auto last = boost::asio::buffer_sequence_end(buffers);

	// TODO: we can do this better - for a multi-element
	// buffer sequence, we could merge some of the data into
	// a single packet
	for (auto it = first; it != last; ++it)
	{
		current_size = 0;
		auto bufsize = it->size();
		while (current_size < bufsize)
		{
			auto size_to_write = static_cast<std::uint32_t>(std::min(
					MAX_PACKET_SIZE,
					bufsize - current_size
			));
			header.packet_size.value = size_to_write;
			header.sequence_number = sequence_number_++;
			header_buffer.clear();
			serialize(header_buffer, header);
			// TODO: we could use a buffer sequence to write these two
			boost::asio::write(next_layer_, boost::asio::buffer(header_buffer.data(), header_buffer.size()));
			boost::asio::write(next_layer_, boost::asio::buffer(*it + current_size, size_to_write));
			current_size += size_to_write;
		}
	}
}

template <typename AsyncStream>
void mysql::MysqlStream<AsyncStream>::handshake(const HandshakeParams& params)
{
	std::vector<std::uint8_t> read_buffer;
	DynamicBuffer write_buffer;

	// Read handshake
	read(read_buffer);
	auto msg_type = get_message_type(read_buffer);
	if (msg_type != handshake_protocol_version_10)
	{
		const char* reason = msg_type == handshake_protocol_version_9 ?
				"Unsupported protocol version 9" :
				"Unknown message type";
		throw std::runtime_error {reason};
	}
	mysql::Handshake handshake;
	deserialize(read_buffer.data()+1, read_buffer.data() + read_buffer.size(), handshake);

	// Process the handshake
	detail::check_capabilities(handshake.capability_falgs);
	detail::check_authentication_method(handshake);
	std::cout << handshake << "\n\n";

	// Response
	mysql::HandshakeResponse handshake_response;
	mysql_native_password::response_buffer auth_response;
	mysql_native_password::compute_auth_string(params.password, handshake.auth_plugin_data.data(), auth_response);
	handshake_response.client_flag = detail::BASIC_CAPABILITIES_FLAGS;
	handshake_response.max_packet_size = 0xffff;
	handshake_response.character_set = params.character_set;
	handshake_response.username.value = params.username;
	handshake_response.auth_response.value = std::string_view {(const char*)auth_response, sizeof(auth_response)};
	handshake_response.client_plugin_name.value = "mysql_native_password";
	handshake_response.database.value = params.database;
	std::cout << handshake_response << "\n\n";

	// Serialize and send
	serialize(write_buffer, handshake_response);
	write(write_buffer.get());

	// TODO: support auth mismatch
	// TODO: support SSL

	// Read the OK/ERR
	read(read_buffer);
	msg_type = get_message_type(read_buffer);
	if (msg_type != ok_packet_header && msg_type != eof_packet_header)
	{
		throw std::runtime_error { "Unknown message type" };
	}
	std::cout << "Connected to server\n";
}

// Async
template <typename AsyncStream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(void))
mysql::MysqlStream<AsyncStream>::async_read(
	std::vector<std::uint8_t>& buffer,
	CompletionToken&& token
)
{
	using HandlerSignature = void(void);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename AsyncStream::executor_type>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op: BaseType, boost::asio::coroutine
	{
		MysqlStream<AsyncStream>& stream_;
		std::vector<std::uint8_t>& buffer_;
		std::size_t current_size = 0;

		Op(HandlerType&& handler, MysqlStream<AsyncStream>& stream, std::vector<std::uint8_t>& buffer):
			BaseType(std::move(handler), stream.next_layer_.get_executor()),
			stream_(stream), buffer_(buffer)
		{
		}

		// Returns size to read
		std::uint32_t process_header(ReadIterator first, ReadIterator last)
		{
			PacketHeader header;
			deserialize(first, last, header);
			stream_.process_sequence_number(header.sequence_number);
			buffer_.resize(current_size + header.packet_size.value);
			return header.packet_size.value;
		}

		void operator()(boost::system::error_code ec,
				std::size_t bytes_transferred, bool cont=true)
		{
			// TODO: error handling
			auto& header_buffer = stream_.header_read_buffer_;
			std::uint32_t size_to_read;

			reenter(*this)
			{
				do
				{
					yield boost::asio::async_read(
							stream_.next_layer_,
							boost::asio::buffer(header_buffer),
							std::move(*this)
					);

					size_to_read = process_header(std::begin(header_buffer), std::end(header_buffer));

					yield boost::asio::async_read(
							stream_.next_layer_,
							boost::asio::buffer(buffer_.data() + current_size, size_to_read),
							std::move(*this)
					);

					current_size += bytes_transferred;
				} while (bytes_transferred == 0xffffff);

				this->complete(cont);
			}
		}
	};

	Op(std::move(initiator.completion_handler), *this, buffer)(boost::system::error_code{}, 0, false);
	return initiator.result.get();
}

template <typename AsyncStream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(void))
mysql::MysqlStream<AsyncStream>::async_write(
	boost::asio::const_buffer buffer,
	CompletionToken&& token
)
{
	using HandlerSignature = void(void);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename AsyncStream::executor_type>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op : BaseType, boost::asio::coroutine
	{
		MysqlStream<AsyncStream>& stream_;
		boost::asio::const_buffer buffer_;
		std::size_t current_size = 0;

		Op(HandlerType&& handler, MysqlStream<AsyncStream>& stream,
				boost::asio::const_buffer buffer):
			BaseType(std::move(handler), stream.next_layer_.get_executor()),
			stream_(stream),
			buffer_(buffer)
		{
		}

		void operator()(boost::system::error_code ec,
				std::size_t bytes_transferred, bool cont=true)
		{
			// TODO: error handling
			constexpr std::size_t MAX_PACKET_SIZE = 0xffffff;
			std::size_t size_to_write;
			PacketHeader header;
			auto& header_buffer = stream_.header_write_buffer_;

			reenter(*this)
			{
				while (current_size < buffer_.size())
				{
					size_to_write = static_cast<std::uint32_t>(std::min(
							MAX_PACKET_SIZE,
							buffer_.size() - current_size
					));

					header.packet_size.value = size_to_write;
					header.sequence_number = stream_.sequence_number_++;
					header_buffer.clear();
					serialize(header_buffer, header);

					yield boost::asio::async_write(
							stream_.next_layer_,
							std::array<boost::asio::const_buffer, 2> {
									boost::asio::buffer(header_buffer.data(), header_buffer.size()),
									boost::asio::buffer(buffer_ + current_size, size_to_write)
							},
							std::move(*this)
					);

					current_size += (bytes_transferred - 4);
				}

				this->complete(cont);
			}
		}

	};

	Op(std::move(initiator.completion_handler), *this, buffer)(boost::system::error_code{}, 0, false);
	return initiator.result.get();
}

template <typename AsyncStream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(void))
mysql::MysqlStream<AsyncStream>::async_handshake(
	const HandshakeParams& params,
	CompletionToken&& token
)
{
	using HandlerSignature = void(void);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename AsyncStream::executor_type>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op : BaseType, boost::asio::coroutine
	{
		MysqlStream<AsyncStream>& stream_;
		const HandshakeParams& params_;

		Op(HandlerType&& handler, MysqlStream<AsyncStream>& stream,
				const HandshakeParams& params):
			BaseType(std::move(handler), stream.next_layer_.get_executor()),
			stream_(stream),
			params_(params)
		{
		}

		void process_handshake(
			const std::vector<std::uint8_t>& handshake_buffer,
			DynamicBuffer& handshake_response_buffer
		) const
		{
			mysql::Handshake handshake;
			mysql::HandshakeResponse handshake_response;

			auto msg_type = get_message_type(handshake_buffer);
			if (msg_type != handshake_protocol_version_10)
			{
				const char* reason = msg_type == handshake_protocol_version_9 ?
						"Unsupported protocol version 9" :
						"Unknown message type";
				throw std::runtime_error {reason};
			}

			deserialize(handshake_buffer.data()+1, handshake_buffer.data() + handshake_buffer.size(), handshake);

			// Process the handshake
			detail::check_capabilities(handshake.capability_falgs);
			detail::check_authentication_method(handshake);
			std::cout << handshake << "\n\n";

			// Response
			mysql_native_password::response_buffer auth_response;
			mysql_native_password::compute_auth_string(
					params_.password,
					handshake.auth_plugin_data.data(),
					auth_response
			);
			handshake_response.client_flag = detail::BASIC_CAPABILITIES_FLAGS;
			handshake_response.max_packet_size = 0xffff;
			handshake_response.character_set = params_.character_set;
			handshake_response.username.value = params_.username;
			handshake_response.auth_response.value =
					std::string_view {(const char*)auth_response, sizeof(auth_response)};
			handshake_response.client_plugin_name.value = "mysql_native_password";
			handshake_response.database.value = params_.database;
			std::cout << handshake_response << "\n\n";

			// Serialize response
			serialize(handshake_response_buffer, handshake_response);
		}

		void operator()(bool cont=true)
		{
			std::uint8_t msg_type;
			auto& read_buffer = stream_.read_buffer_;
			auto& write_buffer = stream_.write_buffer_;

			// TODO: error handling
			reenter(*this)
			{
				// Read handshake
				yield stream_.async_read(read_buffer, std::move(*this));

				process_handshake(read_buffer, write_buffer);

				yield stream_.async_write(
						boost::asio::buffer(write_buffer.data(), write_buffer.size()),
						std::move(*this)
				);

				// TODO: support auth mismatch
				// TODO: support SSL

				// Read the OK/ERR
				yield stream_.async_read(read_buffer, std::move(*this));
				msg_type = get_message_type(read_buffer);
				if (msg_type != ok_packet_header && msg_type != eof_packet_header)
				{
					throw std::runtime_error { "Unknown message type" };
				}

				this->complete(cont);
			}
		}

	};

	Op(std::move(initiator.completion_handler), *this, params)(false);
	return initiator.result.get();
}


#endif
