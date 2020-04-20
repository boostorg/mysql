//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/system/system_error.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include "boost/mysql/detail/protocol/channel.hpp"
#include "test_common.hpp"

using namespace testing;
using namespace boost::mysql::detail;
using namespace boost::asio;
using boost::mysql::error_code;
using boost::mysql::errc;
using boost::mysql::test::concat;

namespace
{

class MockStream
{
public:
    MOCK_METHOD2(read_buffer, std::size_t(boost::asio::mutable_buffer, error_code&));
    MOCK_METHOD2(write_buffer, std::size_t(boost::asio::const_buffer, error_code&));

    void set_default_behavior()
    {
        ON_CALL(*this, read_buffer).WillByDefault(DoAll(
            SetArgReferee<1>(make_error_code(boost::system::errc::timed_out)),
            Return(0)
        ));
        ON_CALL(*this, write_buffer).WillByDefault(DoAll(
            SetArgReferee<1>(make_error_code(boost::system::errc::timed_out)),
            Return(0)
        ));
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(MutableBufferSequence mb, error_code& ec)
    {
        if (buffer_size(mb) == 0)
        {
            ec.clear();
            return 0;
        }
        size_t res = 0;
        for (auto it = buffer_sequence_begin(mb); it != buffer_sequence_end(mb); ++it)
            res += read_buffer(*it, ec);
        return res;
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(MutableBufferSequence mb)
    {
        error_code ec;
        auto res = read_some(mb, ec);
        if (res)
        {
            throw boost::system::system_error(ec);
        }
        return res;
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(ConstBufferSequence cb, error_code& ec)
    {
        if (buffer_size(cb) == 0)
        {
            ec.clear();
            return 0;
        }
        size_t res = 0;
        for (auto it = buffer_sequence_begin(cb); it != buffer_sequence_end(cb); ++it)
        {
            auto written = write_buffer(*it, ec);
            res += written;
            if (written < it->size()) break;
        }
        return res;
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(ConstBufferSequence mb)
    {
        error_code ec;
        auto res = write_some(mb, ec);
        if (res)
        {
            throw boost::system::system_error(ec);
        }
        return res;
    }

    using lowest_layer_type = MockStream;
    using executor_type = boost::asio::system_executor;

    MockStream& lowest_layer() { return *this; }
    executor_type get_executor() { return boost::asio::system_executor(); }
};

struct MysqlChannelFixture : public Test
{
    using MockChannel = channel<NiceMock<MockStream>>;
    NiceMock<MockStream> stream;
    MockChannel chan {stream};
    error_code code;
    InSequence seq;

    MysqlChannelFixture()
    {
        stream.set_default_behavior();
    }
};


struct MysqlChannelReadTest : public MysqlChannelFixture
{
    std::vector<uint8_t> buffer { 0xab, 0xac, 0xad, 0xae }; // simulate buffer was not empty, to verify we clear it
    std::vector<uint8_t> bytes_to_read;
    std::size_t index {0};

    void verify_buffer(const std::vector<uint8_t>& expected)
    {
        EXPECT_EQ(buffer, expected);
    }

    static auto buffer_copier(const std::vector<uint8_t>& buffer)
    {
        return [buffer](boost::asio::mutable_buffer b, error_code& ec) {
            assert(b.size() >= buffer.size());
            memcpy(b.data(), buffer.data(), buffer.size());
            ec.clear();
            return buffer.size();
        };
    }

    auto make_read_handler()
    {
        return [this](boost::asio::mutable_buffer b, error_code& ec) {
            std::size_t to_copy = std::min(b.size(), bytes_to_read.size() - index);
            memcpy(b.data(), bytes_to_read.data() + index, to_copy);
            index += to_copy;
            ec.clear();
            return to_copy;
        };
    }

    static auto read_failer(error_code error)
    {
        return [error](boost::asio::mutable_buffer, error_code& ec) {
            ec = error;
            return size_t(0);
        };
    }
};

TEST_F(MysqlChannelReadTest, SyncRead_AllReadsSuccessful_ReadHeaderPopulatesBuffer)
{
    ON_CALL(stream, read_buffer)
        .WillByDefault(Invoke(make_read_handler()));
    bytes_to_read = {
        0x03, 0x00, 0x00, 0x00,
        0xfe, 0x03, 0x02
    };
    chan.read(buffer, code);
    EXPECT_EQ(code, error_code());
    verify_buffer({0xfe, 0x03, 0x02});
}

TEST_F(MysqlChannelReadTest, SyncRead_MoreThan16M_JoinsPackets)
{
    ON_CALL(stream, read_buffer)
        .WillByDefault(Invoke(make_read_handler()));
    concat(bytes_to_read, {0xff, 0xff, 0xff, 0x00});
    concat(bytes_to_read, std::vector<uint8_t>(0xffffff, 0x20));
    concat(bytes_to_read, {0xff, 0xff, 0xff, 0x01});
    concat(bytes_to_read, std::vector<uint8_t>(0xffffff, 0x20));
    concat(bytes_to_read, {0x04, 0x00, 0x00, 0x02});
    concat(bytes_to_read, {0x20, 0x20, 0x20, 0x20});
    chan.read(buffer, code);
    EXPECT_EQ(code, error_code());
    verify_buffer(std::vector<uint8_t>(0xffffff * 2 + 4, 0x20));
}

TEST_F(MysqlChannelReadTest, SyncRead_EmptyPacket_LeavesBufferEmpty)
{
    ON_CALL(stream, read_buffer)
        .WillByDefault(Invoke(make_read_handler()));
    concat(bytes_to_read, {0x00, 0x00, 0x00, 0x00});
    chan.read(buffer, code);
    EXPECT_EQ(code, error_code());
    verify_buffer(std::vector<uint8_t>{});
}

TEST_F(MysqlChannelReadTest, SyncRead_ShortReads_InvokesReadAgain)
{
    EXPECT_CALL(stream, read_buffer)
        .WillOnce(Invoke(buffer_copier({0x04})))
        .WillOnce(Invoke(buffer_copier({     0x00, 0x00, 0x00})))
        .WillOnce(Invoke(buffer_copier({0x01, 0x02})))
        .WillOnce(Invoke(buffer_copier({0x03, 0x04})));
    chan.read(buffer, code);
    EXPECT_EQ(code, error_code());
    verify_buffer({0x01, 0x02, 0x03, 0x04});
}

TEST_F(MysqlChannelReadTest, SyncRead_ReadErrorInHeader_ReturnsFailureErrorCode)
{
    auto expected_error = make_error_code(boost::system::errc::not_supported);
    EXPECT_CALL(stream, read_buffer)
        .WillOnce(Invoke(read_failer(expected_error)));
    chan.read(buffer, code);
    EXPECT_EQ(code, expected_error);
}

TEST_F(MysqlChannelReadTest, SyncRead_ReadErrorInPacket_ReturnsFailureErrorCode)
{
    auto expected_error = make_error_code(boost::system::errc::not_supported);
    EXPECT_CALL(stream, read_buffer)
        .WillOnce(Invoke(buffer_copier({0xff, 0xff, 0xff, 0x00})))
        .WillOnce(Invoke(read_failer(expected_error)));
    chan.read(buffer, code);
    EXPECT_EQ(code, expected_error);
}

TEST_F(MysqlChannelReadTest, SyncRead_SequenceNumberMismatch_ReturnsAppropriateErrorCode)
{
    ON_CALL(stream, read_buffer)
        .WillByDefault(Invoke(make_read_handler()));
    bytes_to_read = {0xff, 0xff, 0xff, 0x05};
    chan.read(buffer, code);
    EXPECT_EQ(code, make_error_code(errc::sequence_number_mismatch));
}

TEST_F(MysqlChannelReadTest, SyncRead_SequenceNumberNotZero_RespectsCurrentSequenceNumber)
{
    ON_CALL(stream, read_buffer)
        .WillByDefault(Invoke(make_read_handler()));
    bytes_to_read = {
        0x03, 0x00, 0x00, 0x21,
        0xfe, 0x03, 0x02
    };
    chan.reset_sequence_number(0x21);
    chan.read(buffer, code);
    EXPECT_EQ(code, error_code());
    verify_buffer({0xfe, 0x03, 0x02});
    EXPECT_EQ(chan.sequence_number(), 0x22);
}

TEST_F(MysqlChannelReadTest, SyncRead_SequenceNumberFF_SequenceNumberWraps)
{
    ON_CALL(stream, read_buffer)
        .WillByDefault(Invoke(make_read_handler()));
    bytes_to_read = {
        0x03, 0x00, 0x00, 0xff,
        0xfe, 0x03, 0x02
    };
    chan.reset_sequence_number(0xff);
    chan.read(buffer, code);
    EXPECT_EQ(code, error_code());
    verify_buffer({0xfe, 0x03, 0x02});
    EXPECT_EQ(chan.sequence_number(), 0);
}


struct MysqlChannelWriteTest : public MysqlChannelFixture
{
    std::vector<uint8_t> bytes_written;

    void verify_buffer(const std::vector<uint8_t>& expected)
    {
        EXPECT_EQ(bytes_written, expected);
    }

    auto make_write_handler(std::size_t max_bytes_written = 0xffffffff)
    {
        return [this, max_bytes_written](boost::asio::const_buffer buff, error_code& ec) {
            auto actual_size = std::min(buff.size(), max_bytes_written);
            concat(bytes_written, boost::asio::buffer(buff.data(), actual_size));
            ec.clear();
            return actual_size;
        };
    }

    static auto write_failer(boost::system::errc::errc_t error)
    {
        return [error](boost::asio::const_buffer, error_code& ec) {
            ec = make_error_code(error);
            return 0;
        };
    }
};

TEST_F(MysqlChannelWriteTest, SyncWrite_AllWritesSuccessful_WritesHeaderAndBuffer)
{
    ON_CALL(stream, write_buffer)
        .WillByDefault(Invoke(make_write_handler()));
    chan.write(buffer(std::vector<uint8_t>{0xaa, 0xab, 0xac}), code);
    verify_buffer({
        0x03, 0x00, 0x00, 0x00, // header
        0xaa, 0xab, 0xac // body
    });
    EXPECT_EQ(code, error_code());
}

TEST_F(MysqlChannelWriteTest, SyncWrite_MoreThan16M_SplitsInPackets)
{
    ON_CALL(stream, write_buffer)
        .WillByDefault(Invoke(make_write_handler()));
    chan.write(buffer(std::vector<uint8_t>(2*0xffffff + 4, 0xab)), code);
    std::vector<uint8_t> expected_buffer {0xff, 0xff, 0xff, 0x00};
    concat(expected_buffer, std::vector<std::uint8_t>(0xffffff, 0xab));
    concat(expected_buffer, {0xff, 0xff, 0xff, 0x01});
    concat(expected_buffer, std::vector<std::uint8_t>(0xffffff, 0xab));
    concat(expected_buffer, {0x04, 0x00, 0x00, 0x02});
    concat(expected_buffer, std::vector<std::uint8_t>(4, 0xab));
    verify_buffer(expected_buffer);
    EXPECT_EQ(code, error_code());
}

TEST_F(MysqlChannelWriteTest, SyncWrite_EmptyPacket_WritesHeader)
{
    ON_CALL(stream, write_buffer)
        .WillByDefault(Invoke(make_write_handler()));
    chan.reset_sequence_number(2);
    chan.write(buffer(std::vector<uint8_t>{}), code);
    verify_buffer({0x00, 0x00, 0x00, 0x02});
    EXPECT_EQ(code, error_code());
}

TEST_F(MysqlChannelWriteTest, SyncWrite_ShortWrites_WritesHeaderAndBuffer)
{
    ON_CALL(stream, write_buffer)
        .WillByDefault(Invoke(make_write_handler(2)));
    chan.write(buffer(std::vector<uint8_t>{0xaa, 0xab, 0xac}), code);
    verify_buffer({
        0x03, 0x00, 0x00, 0x00, // header
        0xaa, 0xab, 0xac // body
    });
    EXPECT_EQ(code, error_code());
}

TEST_F(MysqlChannelWriteTest, SyncWrite_WriteErrorInHeader_ReturnsErrorCode)
{
    ON_CALL(stream, write_buffer)
        .WillByDefault(Invoke(write_failer(boost::system::errc::broken_pipe)));
    chan.write(buffer(std::vector<uint8_t>(10, 0x01)), code);
    EXPECT_EQ(code, make_error_code(boost::system::errc::broken_pipe));
}

TEST_F(MysqlChannelWriteTest, SyncWrite_WriteErrorInPacket_ReturnsErrorCode)
{
    EXPECT_CALL(stream, write_buffer)
        .WillOnce(Return(4))
        .WillOnce(Invoke(write_failer(boost::system::errc::broken_pipe)));
    chan.write(buffer(std::vector<uint8_t>(10, 0x01)), code);
    EXPECT_EQ(code, make_error_code(boost::system::errc::broken_pipe));
}

TEST_F(MysqlChannelWriteTest, SyncWrite_SequenceNumberNotZero_RespectsSequenceNumber)
{
    chan.reset_sequence_number(0xab);
    ON_CALL(stream, write_buffer)
        .WillByDefault(Invoke(make_write_handler()));
    chan.write(buffer(std::vector<uint8_t>{0xaa, 0xab, 0xac}), code);
    verify_buffer({
        0x03, 0x00, 0x00, 0xab, // header
        0xaa, 0xab, 0xac // body
    });
    EXPECT_EQ(code, error_code());
    EXPECT_EQ(chan.sequence_number(), 0xac);
}

TEST_F(MysqlChannelWriteTest, SyncWrite_SequenceIsFF_WrapsSequenceNumber)
{
    chan.reset_sequence_number(0xff);
    ON_CALL(stream, write_buffer)
        .WillByDefault(Invoke(make_write_handler()));
    chan.write(buffer(std::vector<uint8_t>{0xaa, 0xab, 0xac}), code);
    verify_buffer({
        0x03, 0x00, 0x00, 0xff, // header
        0xaa, 0xab, 0xac // body
    });
    EXPECT_EQ(code, error_code());
    EXPECT_EQ(chan.sequence_number(), 0);
}

} // anon namespace

