#ifndef MYSQL_ASIO_IMPL_CHANNEL_IPP
#define MYSQL_ASIO_IMPL_CHANNEL_IPP

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/async_base.hpp>
#include <cassert>
#include "mysql/impl/messages.hpp"
#include "mysql/impl/constants.hpp"
#include <boost/asio/yield.hpp>

namespace mysql
{
namespace detail
{

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

template <typename AsyncStream>
bool mysql::detail::channel<AsyncStream>::process_sequence_number(
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
mysql::error_code mysql::detail::channel<AsyncStream>::process_header_read(
	std::uint32_t& size_to_read
)
{
	packet_header header;
	DeserializationContext ctx (boost::asio::buffer(header_buffer_), capabilities(0)); // unaffected by capabilities
	[[maybe_unused]] Error err = deserialize(header, ctx);
	assert(err == Error::ok); // this should always succeed
	if (!process_sequence_number(header.sequence_number.value))
	{
		return make_error_code(Error::sequence_number_mismatch);
	}
	size_to_read = header.packet_size.value;
	return error_code();
}

template <typename AsyncStream>
void mysql::detail::channel<AsyncStream>::process_header_write(
	std::uint32_t size_to_write
)
{
	packet_header header;
	header.packet_size.value = size_to_write;
	header.sequence_number.value = next_sequence_number();
	SerializationContext ctx (capabilities(0), header_buffer_.data()); // capabilities not relevant here
	serialize(header, ctx);
}

template <typename AsyncStream>
template <typename Allocator>
void mysql::detail::channel<AsyncStream>::read(
	basic_bytestring<Allocator>& buffer,
	error_code& errc
)
{
	std::size_t transferred_size = 0;
	std::uint32_t size_to_read = 0;
	buffer.clear();
	errc.clear();

	do
	{
		boost::asio::read(
			next_layer_,
			boost::asio::buffer(header_buffer_),
			errc
		);
		if (errc) return;
		errc = process_header_read(size_to_read);
		if (errc) return;
		buffer.resize(buffer.size() + size_to_read);
		boost::asio::read(
			next_layer_,
			boost::asio::buffer(buffer.data() + transferred_size, size_to_read),
			errc
		);
		if (errc) return;
		transferred_size += size_to_read;
	} while (size_to_read == MAX_PACKET_SIZE);
}

template <typename AsyncStream>
void mysql::detail::channel<AsyncStream>::write(
	boost::asio::const_buffer buffer,
	error_code& errc
)
{
	std::size_t transferred_size = 0;
	auto bufsize = buffer.size();
	auto first = static_cast<ReadIterator>(buffer.data());

	while (transferred_size < bufsize)
	{
		auto size_to_write = compute_size_to_write(bufsize, transferred_size);
		process_header_write(size_to_write);
		boost::asio::write(
			next_layer_,
			std::array<boost::asio::const_buffer, 2> {
				boost::asio::buffer(header_buffer_),
				boost::asio::buffer(first + transferred_size, size_to_write)
			},
			errc
		);
		if (errc) return;
		transferred_size += size_to_write;
	}
}


template <typename AsyncStream>
template <typename Allocator, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code))
mysql::detail::channel<AsyncStream>::async_read(
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
			error_code errc,
			std::size_t bytes_transferred,
			bool cont=true
		)
		{
			std::uint32_t size_to_read = 0;

			reenter(*this)
			{
				do
				{
					yield boost::asio::async_read(
						stream_.next_layer_,
						boost::asio::buffer(stream_.header_buffer_),
						std::move(*this)
					);

					if (errc)
					{
						this->complete(cont, errc);
						yield break;
					}

					errc = stream_.process_header_read(size_to_read);

					if (errc)
					{
						this->complete(cont, errc);
						yield break;
					}

					buffer_.resize(buffer_.size() + size_to_read);

					yield boost::asio::async_read(
						stream_.next_layer_,
						boost::asio::buffer(buffer_.data() + total_transferred_size_, size_to_read),
						std::move(*this)
					);

					if (errc)
					{
						this->complete(cont, errc);
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
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code))
mysql::detail::channel<AsyncStream>::async_write(
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
			error_code errc,
			std::size_t bytes_transferred,
			bool cont=true
		)
		{
			std::uint32_t size_to_write;

			reenter(*this)
			{
				while (total_transferred_size_ < buffer_.size())
				{
					size_to_write = compute_size_to_write(buffer_.size(), total_transferred_size_);
					stream_.process_header_write(size_to_write);

					yield boost::asio::async_write(
						stream_.next_layer_,
						std::array<boost::asio::const_buffer, 2> {
							boost::asio::buffer(stream_.header_buffer_),
							boost::asio::buffer(buffer_ + total_transferred_size_, size_to_write)
						},
						std::move(*this)
					);

					if (errc)
					{
						this->complete(cont, errc);
						yield break;
					}

					total_transferred_size_ += (bytes_transferred - 4); // header size
				}

				this->complete(cont, error_code());
			}
		}

	};

	Op(std::move(initiator.completion_handler), *this, buffer)(error_code(), 0, false);
	return initiator.result.get();
}

#include <boost/asio/unyield.hpp>


#endif
