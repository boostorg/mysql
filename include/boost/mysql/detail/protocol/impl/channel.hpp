#ifndef MYSQL_ASIO_IMPL_CHANNEL_IPP
#define MYSQL_ASIO_IMPL_CHANNEL_IPP

#include <boost/beast/core/async_base.hpp>
#include <cassert>
#include "boost/mysql/detail/protocol/common_messages.hpp"
#include "boost/mysql/detail/protocol/constants.hpp"
#include <boost/asio/yield.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline std::uint32_t compute_size_to_write(
	std::size_t buffer_size,
	std::size_t transferred_size
)
{
	return static_cast<std::uint32_t>(std::min(
		MAX_PACKET_SIZE,
		buffer_size - transferred_size
	));
}

} // detail
} // mysql
} // boost

template <typename AsyncStream>
bool boost::mysql::detail::channel<AsyncStream>::process_sequence_number(
	std::uint8_t got
)
{
	if (got == sequence_number_)
	{
		++sequence_number_;
		return true;
	}
	else
	{
		return false;
	}
}

template <typename AsyncStream>
boost::mysql::error_code boost::mysql::detail::channel<AsyncStream>::process_header_read(
	std::uint32_t& size_to_read
)
{
	packet_header header;
	deserialization_context ctx (boost::asio::buffer(header_buffer_), capabilities(0)); // unaffected by capabilities
	[[maybe_unused]] errc err = deserialize(header, ctx);
	assert(err == errc::ok); // this should always succeed
	if (!process_sequence_number(header.sequence_number.value))
	{
		return make_error_code(errc::sequence_number_mismatch);
	}
	size_to_read = header.packet_size.value;
	return error_code();
}

template <typename AsyncStream>
void boost::mysql::detail::channel<AsyncStream>::process_header_write(
	std::uint32_t size_to_write
)
{
	packet_header header;
	header.packet_size.value = size_to_write;
	header.sequence_number.value = next_sequence_number();
	serialization_context ctx (capabilities(0), header_buffer_.data()); // capabilities not relevant here
	serialize(header, ctx);
}

template <typename AsyncStream>
template <typename BufferSeq>
std::size_t boost::mysql::detail::channel<AsyncStream>::read_impl(
	BufferSeq&& buff,
	error_code& ec
)
{
	if (ssl_active_)
	{
		return boost::asio::read(next_layer_, std::forward<BufferSeq>(buff), ec);
	}
	else
	{
		return boost::asio::read(next_layer_.next_layer(), std::forward<BufferSeq>(buff), ec);
	}
}

template <typename AsyncStream>
template <typename BufferSeq>
std::size_t boost::mysql::detail::channel<AsyncStream>::write_impl(
	BufferSeq&& buff,
	error_code& ec
)
{
	if (ssl_active_)
	{
		return boost::asio::write(next_layer_, std::forward<BufferSeq>(buff), ec);
	}
	else
	{
		return boost::asio::write(next_layer_.next_layer(), std::forward<BufferSeq>(buff), ec);
	}
}

template <typename AsyncStream>
template <typename BufferSeq, typename CompletionToken>
auto boost::mysql::detail::channel<AsyncStream>::async_read_impl(
	BufferSeq&& buff,
	CompletionToken&& token
)
{
	if (ssl_active_)
	{
		return boost::asio::async_read(
			next_layer_,
			std::forward<BufferSeq>(buff),
			std::forward<CompletionToken>(token)
		);
	}
	else
	{
		return boost::asio::async_read(
			next_layer_.next_layer(),
			std::forward<BufferSeq>(buff),
			std::forward<CompletionToken>(token)
		);
	}
}

template <typename AsyncStream>
template <typename BufferSeq, typename CompletionToken>
auto boost::mysql::detail::channel<AsyncStream>::async_write_impl(
	BufferSeq&& buff,
	CompletionToken&& token
)
{
	if (ssl_active_)
	{
		return boost::asio::async_write(
			next_layer_,
			std::forward<BufferSeq>(buff),
			std::forward<CompletionToken>(token)
		);
	}
	else
	{
		return boost::asio::async_write(
			next_layer_.next_layer(),
			std::forward<BufferSeq>(buff),
			std::forward<CompletionToken>(token)
		);
	}
}

template <typename AsyncStream>
template <typename Allocator>
void boost::mysql::detail::channel<AsyncStream>::read(
	basic_bytestring<Allocator>& buffer,
	error_code& code
)
{
	std::size_t transferred_size = 0;
	std::uint32_t size_to_read = 0;
	buffer.clear();
	code.clear();

	do
	{
		read_impl(boost::asio::buffer(header_buffer_), code);
		if (code) return;
		code = process_header_read(size_to_read);
		if (code) return;
		buffer.resize(buffer.size() + size_to_read);
		read_impl(boost::asio::buffer(buffer.data() + transferred_size, size_to_read), code);
		if (code) return;
		transferred_size += size_to_read;
	} while (size_to_read == MAX_PACKET_SIZE);
}

template <typename AsyncStream>
void boost::mysql::detail::channel<AsyncStream>::write(
	boost::asio::const_buffer buffer,
	error_code& code
)
{
	std::size_t transferred_size = 0;
	auto bufsize = buffer.size();
	auto first = static_cast<const std::uint8_t*>(buffer.data());

	// If the packet is empty, we should still write the header, saying
	// we are sending an empty packet.
	do
	{
		auto size_to_write = compute_size_to_write(bufsize, transferred_size);
		process_header_write(size_to_write);
		write_impl(
			std::array<boost::asio::const_buffer, 2> {
				boost::asio::buffer(header_buffer_),
				boost::asio::buffer(first + transferred_size, size_to_write)
			},
			code
		);
		if (code) return;
		transferred_size += size_to_write;
	} while (transferred_size < bufsize);
}


template <typename AsyncStream>
template <typename Allocator, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::channel<AsyncStream>::async_read(
	basic_bytestring<Allocator>& buffer,
	CompletionToken&& token
)
{
	using HandlerSignature = void(mysql::error_code);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename AsyncStream::executor_type>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op: BaseType, boost::asio::coroutine
	{
		channel<AsyncStream>& stream_;
		basic_bytestring<Allocator>& buffer_;
		std::size_t total_transferred_size_ = 0;

		Op(
			HandlerType&& handler,
			channel<AsyncStream>& stream,
			basic_bytestring<Allocator>& buffer
		):
			BaseType(std::move(handler), stream.next_layer_.get_executor()),
			stream_(stream),
			buffer_(buffer)
		{
		}

		void operator()(
			error_code code,
			std::size_t bytes_transferred,
			bool cont=true
		)
		{
			std::uint32_t size_to_read = 0;

			reenter(*this)
			{
				do
				{
					yield stream_.async_read_impl(
						boost::asio::buffer(stream_.header_buffer_),
						std::move(*this)
					);

					if (code)
					{
						this->complete(cont, code);
						yield break;
					}

					code = stream_.process_header_read(size_to_read);

					if (code)
					{
						this->complete(cont, code);
						yield break;
					}

					buffer_.resize(buffer_.size() + size_to_read);

					yield stream_.async_read_impl(
						boost::asio::buffer(buffer_.data() + total_transferred_size_, size_to_read),
						std::move(*this)
					);

					if (code)
					{
						this->complete(cont, code);
						yield break;
					}

					total_transferred_size_ += bytes_transferred;
				} while (bytes_transferred == MAX_PACKET_SIZE);

				this->complete(cont, error_code());
			}
		}
	};

	buffer.clear();
	Op(std::move(initiator.completion_handler), *this, buffer)(error_code(), 0, false);
	return initiator.result.get();
}

template <typename AsyncStream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::channel<AsyncStream>::async_write(
	boost::asio::const_buffer buffer,
	CompletionToken&& token
)
{
	using HandlerSignature = void(mysql::error_code);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename AsyncStream::executor_type>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op : BaseType, boost::asio::coroutine
	{
		channel<AsyncStream>& stream_;
		boost::asio::const_buffer buffer_;
		std::size_t total_transferred_size_ = 0;

		Op(
			HandlerType&& handler,
			channel<AsyncStream>& stream,
			boost::asio::const_buffer buffer
		):
			BaseType(std::move(handler), stream.next_layer_.get_executor()),
			stream_(stream),
			buffer_(buffer)
		{
		}

		void operator()(
			error_code code,
			std::size_t bytes_transferred,
			bool cont=true
		)
		{
			std::uint32_t size_to_write;

			reenter(*this)
			{
				// Force write the packet header on an empty packet, at least.
				do
				{
					size_to_write = compute_size_to_write(buffer_.size(), total_transferred_size_);
					stream_.process_header_write(size_to_write);

					yield stream_.async_write_impl(
						std::array<boost::asio::const_buffer, 2> {
							boost::asio::buffer(stream_.header_buffer_),
							boost::asio::buffer(buffer_ + total_transferred_size_, size_to_write)
						},
						std::move(*this)
					);

					if (code)
					{
						this->complete(cont, code);
						yield break;
					}

					total_transferred_size_ += (bytes_transferred - 4); // header size

				} while (total_transferred_size_ < buffer_.size());

				this->complete(cont, error_code());
			}
		}

	};

	Op(std::move(initiator.completion_handler), *this, buffer)(error_code(), 0, false);
	return initiator.result.get();
}

template <typename Stream>
void boost::mysql::detail::channel<Stream>::ssl_handshake(
	error_code& ec
)
{
	ssl_active_ = true;
	next_layer_.handshake(boost::asio::ssl::stream_base::client, ec);
}

template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::channel<Stream>::async_ssl_handshake(
	CompletionToken&& token
)
{
	ssl_active_ = true;
	return next_layer_.async_handshake(
		boost::asio::ssl::stream_base::client,
		std::forward<CompletionToken>(token)
	);
}

#include <boost/asio/unyield.hpp>


#endif
