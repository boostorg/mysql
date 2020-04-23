//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_CHANNEL_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_CHANNEL_HPP

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <cassert>
#include "boost/mysql/detail/protocol/common_messages.hpp"
#include "boost/mysql/detail/protocol/constants.hpp"
#include "boost/mysql/detail/auxiliar/valgrind.hpp"

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

template <typename Stream>
bool boost::mysql::detail::channel<Stream>::process_sequence_number(
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

template <typename Stream>
boost::mysql::error_code boost::mysql::detail::channel<Stream>::process_header_read(
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

template <typename Stream>
void boost::mysql::detail::channel<Stream>::process_header_write(
    std::uint32_t size_to_write
)
{
    packet_header header;
    header.packet_size.value = size_to_write;
    header.sequence_number.value = next_sequence_number();
    serialization_context ctx (capabilities(0), header_buffer_.data()); // capabilities not relevant here
    serialize(header, ctx);
}

template <typename Stream>
template <typename BufferSeq>
std::size_t boost::mysql::detail::channel<Stream>::read_impl(
    BufferSeq&& buff,
    error_code& ec
)
{
    if (ssl_active())
    {
        return boost::asio::read(ssl_block_->stream, std::forward<BufferSeq>(buff), ec);
    }
    else
    {
        return boost::asio::read(stream_, std::forward<BufferSeq>(buff), ec);
    }
}

template <typename Stream>
template <typename BufferSeq>
std::size_t boost::mysql::detail::channel<Stream>::write_impl(
    BufferSeq&& buff,
    error_code& ec
)
{
    if (ssl_active())
    {
        return boost::asio::write(ssl_block_->stream, std::forward<BufferSeq>(buff), ec);
    }
    else
    {
        return boost::asio::write(stream_, std::forward<BufferSeq>(buff), ec);
    }
}

template <typename Stream>
template <typename BufferSeq, typename CompletionToken>
auto boost::mysql::detail::channel<Stream>::async_read_impl(
    BufferSeq&& buff,
    CompletionToken&& token
)
{
    if (ssl_active())
    {
        return boost::asio::async_read(
            ssl_block_->stream,
            std::forward<BufferSeq>(buff),
            std::forward<CompletionToken>(token)
        );
    }
    else
    {
        return boost::asio::async_read(
            stream_,
            std::forward<BufferSeq>(buff),
            std::forward<CompletionToken>(token)
        );
    }
}

template <typename Stream>
template <typename BufferSeq, typename CompletionToken>
auto boost::mysql::detail::channel<Stream>::async_write_impl(
    BufferSeq&& buff,
    CompletionToken&& token
)
{
    if (ssl_active())
    {
        return boost::asio::async_write(
            ssl_block_->stream,
            std::forward<BufferSeq>(buff),
            std::forward<CompletionToken>(token)
        );
    }
    else
    {
        return boost::asio::async_write(
            stream_,
            std::forward<BufferSeq>(buff),
            std::forward<CompletionToken>(token)
        );
    }
}

template <typename Stream>
void boost::mysql::detail::channel<Stream>::read(
    bytestring& buffer,
    error_code& code
)
{
    std::size_t transferred_size = 0;
    std::uint32_t size_to_read = 0;
    buffer.clear();
    code.clear();
    auto header_buff = boost::asio::buffer(header_buffer_);

    do
    {
        // Read header
        read_impl(header_buff, code);
        valgrind_make_mem_defined(header_buff);
        if (code) return;

        // See how many bytes we should be reading
        code = process_header_read(size_to_read);
        if (code) return;

        // Read the rest of the message
        buffer.resize(buffer.size() + size_to_read);
        auto read_buffer = boost::asio::buffer(buffer.data() + transferred_size, size_to_read);
        read_impl(read_buffer, code);
        valgrind_make_mem_defined(read_buffer);
        if (code) return;
        transferred_size += size_to_read;

    } while (size_to_read == MAX_PACKET_SIZE);
}

template <typename Stream>
void boost::mysql::detail::channel<Stream>::write(
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


template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::detail::channel<Stream>::read_signature
)
boost::mysql::detail::channel<Stream>::async_read(
    bytestring& buffer,
    CompletionToken&& token
)
{
    struct op : async_op<Stream, CompletionToken, read_signature, op>
    {
        bytestring& buffer_;
        std::size_t total_transferred_size_ = 0;

        op(
            boost::asio::async_completion<CompletionToken, read_signature>& completion,
            channel<Stream>& chan,
            error_info* output_info,
            bytestring& buffer
        ) :
            async_op<Stream, CompletionToken, read_signature, op>(completion, chan, output_info),
            buffer_(buffer)
        {
        }

        void operator()(
            error_code code,
            std::size_t bytes_transferred=0,
            bool cont=true
        )
        {
            // Error checking
            if (code)
            {
                this->complete(cont, code);
                return;
            }

            // Non-error path
            std::uint32_t size_to_read = 0;
            channel<Stream>& chan = this->get_channel();
            BOOST_ASIO_CORO_REENTER(*this)
            {
                do
                {
                    BOOST_ASIO_CORO_YIELD chan.async_read_impl(
                        boost::asio::buffer(chan.header_buffer_),
                        std::move(*this)
                    );
                    valgrind_make_mem_defined(boost::asio::buffer(chan.header_buffer_));

                    code = chan.process_header_read(size_to_read);
                    if (code)
                    {
                        this->complete(cont, code);
                        BOOST_ASIO_CORO_YIELD break;
                    }

                    buffer_.resize(buffer_.size() + size_to_read);

                    BOOST_ASIO_CORO_YIELD chan.async_read_impl(
                        boost::asio::buffer(buffer_.data() + total_transferred_size_, size_to_read),
                        std::move(*this)
                    );
                    valgrind_make_mem_defined(
                        boost::asio::buffer(buffer_.data() + total_transferred_size_, bytes_transferred)
                    );

                    total_transferred_size_ += bytes_transferred;
                } while (bytes_transferred == MAX_PACKET_SIZE);

                this->complete(cont, error_code());
            }
        }
    };

    buffer.clear();
    return op::initiate(std::forward<CompletionToken>(token), *this, nullptr, buffer);
}

template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::detail::channel<Stream>::write_signature
)
boost::mysql::detail::channel<Stream>::async_write(
    boost::asio::const_buffer buffer,
    CompletionToken&& token
)
{
    struct op : async_op<Stream, CompletionToken, write_signature, op>
    {
        boost::asio::const_buffer buffer_;
        std::size_t total_transferred_size_ = 0;

        op(
            boost::asio::async_completion<CompletionToken, write_signature>& completion,
            channel<Stream>& chan,
            error_info* output_info,
            boost::asio::const_buffer buffer
        ) :
            async_op<Stream, CompletionToken, write_signature, op>(completion, chan, output_info),
            buffer_(buffer)
        {
        }

        void operator()(
            error_code code,
            std::size_t bytes_transferred=0,
            bool cont=true
        )
        {
            // Error handling
            if (code)
            {
                this->complete(cont, code);
                return;
            }

            // Non-error path
            std::uint32_t size_to_write;
            channel<Stream>& chan = this->get_channel();
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Force write the packet header on an empty packet, at least.
                do
                {
                    size_to_write = compute_size_to_write(buffer_.size(), total_transferred_size_);
                    chan.process_header_write(size_to_write);

                    BOOST_ASIO_CORO_YIELD chan.async_write_impl(
                        std::array<boost::asio::const_buffer, 2> {
                            boost::asio::buffer(chan.header_buffer_),
                            boost::asio::buffer(buffer_ + total_transferred_size_, size_to_write)
                        },
                        std::move(*this)
                    );

                    total_transferred_size_ += (bytes_transferred - 4); // header size

                } while (total_transferred_size_ < buffer_.size());

                this->complete(cont, error_code());
            }
        }

    };

    return op::initiate(std::forward<CompletionToken>(token), *this, nullptr, buffer);
}

template <typename Stream>
void boost::mysql::detail::channel<Stream>::ssl_handshake(
    error_code& ec
)
{
    create_ssl_block();
    ssl_block_->stream.handshake(boost::asio::ssl::stream_base::client, ec);
}

template <typename Stream>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::detail::channel<Stream>::ssl_handshake_signature
)
boost::mysql::detail::channel<Stream>::async_ssl_handshake(
    CompletionToken&& token
)
{
    create_ssl_block();
    return ssl_block_->stream.async_handshake(
        boost::asio::ssl::stream_base::client,
        std::forward<CompletionToken>(token)
    );
}


#endif
