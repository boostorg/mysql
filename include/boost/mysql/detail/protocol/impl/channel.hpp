//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_CHANNEL_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_CHANNEL_HPP

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/compose.hpp>
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

template <class Stream>
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

template <class Stream>
boost::mysql::error_code boost::mysql::detail::channel<Stream>::process_header_read(
    std::uint32_t& size_to_read
)
{
    packet_header header;
    deserialization_context ctx (boost::asio::buffer(header_buffer_), capabilities(0)); // unaffected by capabilities
    errc err = deserialize(ctx, header);
    if (err != errc::ok)
    {
        return make_error_code(err);
    }
    if (!process_sequence_number(header.sequence_number))
    {
        return make_error_code(errc::sequence_number_mismatch);
    }
    size_to_read = header.packet_size.value;
    return error_code();
}

template <class Stream>
void boost::mysql::detail::channel<Stream>::process_header_write(
    std::uint32_t size_to_write
)
{
    packet_header header;
    header.packet_size.value = size_to_write;
    header.sequence_number = next_sequence_number();
    serialization_context ctx (capabilities(0), header_buffer_.data()); // capabilities not relevant here
    serialize(ctx, header);
}

template <class Stream>
template <class BufferSeq>
std::size_t boost::mysql::detail::channel<Stream>::read_impl(
    BufferSeq&& buff,
    error_code& ec
)
{
    if (ssl_active())
    {
        return boost::asio::read(*ssl_stream_, std::forward<BufferSeq>(buff), ec);
    }
    else
    {
        return boost::asio::read(stream_, std::forward<BufferSeq>(buff), ec);
    }
}

template <class Stream>
template <class BufferSeq>
std::size_t boost::mysql::detail::channel<Stream>::write_impl(
    BufferSeq&& buff,
    error_code& ec
)
{
    if (ssl_active())
    {
        return boost::asio::write(*ssl_stream_, std::forward<BufferSeq>(buff), ec);
    }
    else
    {
        return boost::asio::write(stream_, std::forward<BufferSeq>(buff), ec);
    }
}

template <class Stream>
template <class BufferSeq, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
boost::mysql::detail::channel<Stream>::async_read_impl(
    BufferSeq&& buff,
    CompletionToken&& token
)
{
    if (ssl_active())
    {
        return boost::asio::async_read(
            *ssl_stream_,
            std::forward<BufferSeq>(buff),
            boost::asio::transfer_all(),
            std::forward<CompletionToken>(token)
        );
    }
    else
    {
        return boost::asio::async_read(
            stream_,
            std::forward<BufferSeq>(buff),
            boost::asio::transfer_all(),
            std::forward<CompletionToken>(token)
        );
    }
}

template <class Stream>
template <class BufferSeq, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
boost::mysql::detail::channel<Stream>::async_write_impl(
    BufferSeq&& buff,
    CompletionToken&& token
)
{
    if (ssl_active())
    {
        return boost::asio::async_write(
            *ssl_stream_,
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

template <class Stream>
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
        if (code)
            return;

        // See how many bytes we should be reading
        code = process_header_read(size_to_read);
        if (code)
            return;

        // Read the rest of the message
        buffer.resize(buffer.size() + size_to_read);
        auto read_buffer = boost::asio::buffer(buffer.data() + transferred_size, size_to_read);
        read_impl(read_buffer, code);
        valgrind_make_mem_defined(read_buffer);
        if (code)
            return;
        transferred_size += size_to_read;

    } while (size_to_read == MAX_PACKET_SIZE);
}

template <class Stream>
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
        if (code)
            return;
        transferred_size += size_to_write;
    } while (transferred_size < bufsize);
}

template<class Stream>
struct boost::mysql::detail::channel<Stream>::read_op
    : boost::asio::coroutine
{
    channel<Stream>& chan_;
    bytestring& buffer_;
    std::size_t total_transferred_size_ = 0;

    read_op(
        channel<Stream>& chan,
        bytestring& buffer
    ) :
        chan_(chan),
        buffer_(buffer)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code code = {},
        std::size_t bytes_transferred=0
    )
    {
        // Error checking
        if (code)
        {
            self.complete(code);
            return;
        }

        // Non-error path
        std::uint32_t size_to_read = 0;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            do
            {
                BOOST_ASIO_CORO_YIELD chan_.async_read_impl(
                    boost::asio::buffer(chan_.header_buffer_),
                    std::move(self)
                );
                valgrind_make_mem_defined(boost::asio::buffer(chan_.header_buffer_));

                code = chan_.process_header_read(size_to_read);
                if (code)
                {
                    self.complete(code);
                    BOOST_ASIO_CORO_YIELD break;
                }

                buffer_.resize(buffer_.size() + size_to_read);

                BOOST_ASIO_CORO_YIELD chan_.async_read_impl(
                    boost::asio::buffer(buffer_.data() + total_transferred_size_, size_to_read),
                    std::move(self)
                );
                valgrind_make_mem_defined(
                    boost::asio::buffer(buffer_.data() + total_transferred_size_, bytes_transferred)
                );

                total_transferred_size_ += bytes_transferred;
            } while (bytes_transferred == MAX_PACKET_SIZE);

            self.complete(error_code());
        }
    }
};

template <class Stream>
template <class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::channel<Stream>::async_read(
    bytestring& buffer,
    CompletionToken&& token
)
{
    buffer.clear();
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        read_op(*this, buffer),
        token,
        *this
    );
}

template<class Stream>
struct boost::mysql::detail::channel<Stream>::write_op
    : boost::asio::coroutine
{
    channel<Stream>& chan_;
    boost::asio::const_buffer buffer_;
    std::size_t total_transferred_size_ = 0;

    write_op(
        channel<Stream>& chan,
        boost::asio::const_buffer buffer
    ) :
        chan_(chan),
        buffer_(buffer)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code code = {},
        std::size_t bytes_transferred=0
    )
    {
        // Error handling
        if (code)
        {
            self.complete(code);
            return;
        }

        // Non-error path
        std::uint32_t size_to_write;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Force write the packet header on an empty packet, at least.
            do
            {
                size_to_write = compute_size_to_write(buffer_.size(), total_transferred_size_);
                chan_.process_header_write(size_to_write);

                BOOST_ASIO_CORO_YIELD chan_.async_write_impl(
                    std::array<boost::asio::const_buffer, 2> {
                        boost::asio::buffer(chan_.header_buffer_),
                        boost::asio::buffer(buffer_ + total_transferred_size_, size_to_write)
                    },
                    std::move(self)
                );

                total_transferred_size_ += (bytes_transferred - 4); // header size

            } while (total_transferred_size_ < buffer_.size());

            self.complete(error_code());
        }
    }
};

template <class Stream>
template <class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::channel<Stream>::async_write(
    boost::asio::const_buffer buffer,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        write_op(*this, buffer),
        token,
        *this
    );
}

template <typename Stream>
void boost::mysql::detail::channel<Stream>::create_ssl_stream()
{
    // Determine the context to use
    boost::asio::ssl::context* ctx = external_ctx_;
    if (!ctx)
    {
        local_ctx_.emplace(boost::asio::ssl::context::tls_client);
        ctx = &*local_ctx_;
    }

    // Actually create the stream
    ssl_stream_.emplace(stream_, *ctx);
}

template <class Stream>
void boost::mysql::detail::channel<Stream>::ssl_handshake(
    error_code& ec
)
{
    create_ssl_stream();
    ssl_stream_->handshake(boost::asio::ssl::stream_base::client, ec);
}

template <class Stream>
template <class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code)
)
boost::mysql::detail::channel<Stream>::async_ssl_handshake(
    CompletionToken&& token
)
{
    create_ssl_stream();
    return ssl_stream_->async_handshake(
        boost::asio::ssl::stream_base::client,
        std::forward<CompletionToken>(token)
    );
}

template <class Stream>
boost::mysql::error_code boost::mysql::detail::channel<Stream>::close()
{
    error_code err;
    stream_.shutdown(Stream::shutdown_both, err);
    stream_.close(err);
    return err;
}


#endif
