/*
 * mysql_stream.cpp
 *
 *  Created on: Oct 6, 2019
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/system/system_error.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include "mysql/impl/channel.hpp"

using namespace testing;
using namespace mysql;
using namespace mysql::detail;
using namespace boost::asio;
namespace errc = boost::system::errc;

namespace
{

void concat(std::vector<uint8_t>& lhs, boost::asio::const_buffer rhs)
{
	auto current_size = lhs.size();
	lhs.resize(current_size + rhs.size());
	memcpy(lhs.data() + current_size, rhs.data(), rhs.size());
}

void concat(std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs)
{
	concat(lhs, boost::asio::buffer(rhs));
}

class MockStream
{
public:
	MOCK_METHOD2(read_buffer, std::size_t(boost::asio::mutable_buffer, mysql::error_code&));
	MOCK_METHOD2(write_buffer, std::size_t(boost::asio::const_buffer, mysql::error_code&));

	MockStream()
	{
		ON_CALL(*this, read_buffer).WillByDefault(SetArgReferee<1>(errc::make_error_code(errc::timed_out)));
		ON_CALL(*this, write_buffer).WillByDefault(SetArgReferee<1>(errc::make_error_code(errc::timed_out)));
	}

	template <typename MutableBufferSequence>
	std::size_t read_some(MutableBufferSequence mb, mysql::error_code& ec)
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
	std::size_t write_some(ConstBufferSequence cb, mysql::error_code& ec)
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
};

struct MysqlChannelFixture : public Test
{
	using MockChannel = channel<MockStream>;
	MockStream stream;
	MockChannel chan {stream};
	mysql::error_code errc;
	InSequence seq;
};


struct MysqlChannelReadTest : public MysqlChannelFixture
{
	std::vector<uint8_t> buffer;

	void verify_buffer(const std::vector<uint8_t>& expected)
	{
		EXPECT_EQ(buffer, expected);
	}

	static auto buffer_copier(const std::vector<uint8_t>& buffer)
	{
		return [buffer = move(buffer)](boost::asio::mutable_buffer b, mysql::error_code& ec) {
			memcpy(b.data(), buffer.data(), buffer.size());
			ec.clear();
			return buffer.size();
		};
	}

	static auto read_failer(error_code error)
	{
		return [error](boost::asio::mutable_buffer b, mysql::error_code& ec) {
			ec = error;
			return size_t(0);
		};
	}
};

TEST_F(MysqlChannelReadTest, SyncRead_AllReadsSuccessful_ReadHeaderPopulatesBuffer)
{
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0x03, 0x00, 0x00, 0x00})))
		.WillOnce(Invoke(buffer_copier({0xfe, 0x03, 0x02})));
	chan.read(buffer, errc);
	EXPECT_EQ(errc, error_code());
	verify_buffer({0xfe, 0x03, 0x02});
}

TEST_F(MysqlChannelReadTest, SyncRead_MoreThan16M_JoinsPackets)
{
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0xff, 0xff, 0xff, 0x00})))
		.WillOnce(Invoke(buffer_copier(std::vector<uint8_t>(0xffffff, 0x20))))
		.WillOnce(Invoke(buffer_copier({0xff, 0xff, 0xff, 0x01})))
		.WillOnce(Invoke(buffer_copier(std::vector<uint8_t>(0xffffff, 0x20))))
		.WillOnce(Invoke(buffer_copier({0x04, 0x00, 0x00, 0x02})))
		.WillOnce(Invoke(buffer_copier({0x20, 0x20, 0x20, 0x20})));
	chan.read(buffer, errc);
	EXPECT_EQ(errc, error_code());
	verify_buffer(std::vector<uint8_t>(0xffffff * 2 + 4, 0x20));
}

TEST_F(MysqlChannelReadTest, SyncRead_ShortReads_InvokesReadAgain)
{
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0x04})))
		.WillOnce(Invoke(buffer_copier({     0x00, 0x00, 0x00})))
		.WillOnce(Invoke(buffer_copier({0x01, 0x02})))
		.WillOnce(Invoke(buffer_copier({0x03, 0x04})));
	chan.read(buffer, errc);
	EXPECT_EQ(errc, error_code());
	verify_buffer({0x01, 0x02, 0x03, 0x04});
}

TEST_F(MysqlChannelReadTest, SyncRead_ReadErrorInHeader_ReturnsFailureErrorCode)
{
	auto expected_error = errc::make_error_code(errc::not_supported);
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(read_failer(expected_error)));
	chan.read(buffer, errc);
	EXPECT_EQ(errc, expected_error);
}

TEST_F(MysqlChannelReadTest, SyncRead_ReadErrorInPacket_ReturnsFailureErrorCode)
{
	auto expected_error = errc::make_error_code(errc::not_supported);
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0xff, 0xff, 0xff, 0x00})))
		.WillOnce(Invoke(read_failer(expected_error)));
	chan.read(buffer, errc);
	EXPECT_EQ(errc, expected_error);
}

TEST_F(MysqlChannelReadTest, SyncRead_SequenceNumberMismatch_ReturnsAppropriateErrorCode)
{
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0xff, 0xff, 0xff, 0x05})));
	chan.read(buffer, errc);
	EXPECT_EQ(errc, make_error_code(Error::sequence_number_mismatch));
}

TEST_F(MysqlChannelReadTest, SyncRead_SequenceNumberNotZero_RespectsCurrentSequenceNumber)
{
	chan.reset_sequence_number(0x21);
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0x03, 0x00, 0x00, 0x21})))
		.WillOnce(Invoke(buffer_copier({0xfe, 0x03, 0x02})));
	chan.read(buffer, errc);
	EXPECT_EQ(errc, error_code());
	verify_buffer({0xfe, 0x03, 0x02});
	EXPECT_EQ(chan.sequence_number(), 0x22);
}

TEST_F(MysqlChannelReadTest, SyncRead_SequenceNumberFF_SequenceNumberWraps)
{
	chan.reset_sequence_number(0xff);
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0x03, 0x00, 0x00, 0xff})))
		.WillOnce(Invoke(buffer_copier({0xfe, 0x03, 0x02})));
	chan.read(buffer, errc);
	EXPECT_EQ(errc, error_code());
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

	static auto write_failer(errc::errc_t error)
	{
		return [error](boost::asio::const_buffer buff, error_code& ec) {
			ec = errc::make_error_code(error);
			return 0;
		};
	}
};

TEST_F(MysqlChannelWriteTest, SyncWrite_AllWritesSuccessful_WritesHeaderAndBuffer)
{
	ON_CALL(stream, write_buffer)
		.WillByDefault(Invoke(make_write_handler()));
	chan.write(buffer(std::vector<uint8_t>{0xaa, 0xab, 0xac}), errc);
	verify_buffer({
		0x03, 0x00, 0x00, 0x00, // header
		0xaa, 0xab, 0xac // body
	});
	EXPECT_EQ(errc, error_code());
}

TEST_F(MysqlChannelWriteTest, SyncWrite_MoreThan16M_SplitsInPackets)
{
	ON_CALL(stream, write_buffer)
		.WillByDefault(Invoke(make_write_handler()));
	chan.write(buffer(std::vector<uint8_t>(2*0xffffff + 4, 0xab)), errc);
	std::vector<uint8_t> expected_buffer {0xff, 0xff, 0xff, 0x00};
	concat(expected_buffer, std::vector<std::uint8_t>(0xffffff, 0xab));
	concat(expected_buffer, {0xff, 0xff, 0xff, 0x01});
	concat(expected_buffer, std::vector<std::uint8_t>(0xffffff, 0xab));
	concat(expected_buffer, {0x04, 0x00, 0x00, 0x02});
	concat(expected_buffer, std::vector<std::uint8_t>(4, 0xab));
	verify_buffer(expected_buffer);
	EXPECT_EQ(errc, error_code());
}

TEST_F(MysqlChannelWriteTest, SyncWrite_ShortWrites_WritesHeaderAndBuffer)
{
	ON_CALL(stream, write_buffer)
		.WillByDefault(Invoke(make_write_handler(2)));
	chan.write(buffer(std::vector<uint8_t>{0xaa, 0xab, 0xac}), errc);
	verify_buffer({
		0x03, 0x00, 0x00, 0x00, // header
		0xaa, 0xab, 0xac // body
	});
	EXPECT_EQ(errc, error_code());
}

TEST_F(MysqlChannelWriteTest, SyncWrite_WriteErrorInHeader_ReturnsErrorCode)
{
	ON_CALL(stream, write_buffer)
		.WillByDefault(Invoke(write_failer(errc::broken_pipe)));
	chan.write(buffer(std::vector<uint8_t>(10, 0x01)), errc);
	EXPECT_EQ(errc, errc::make_error_code(errc::broken_pipe));
}

TEST_F(MysqlChannelWriteTest, SyncWrite_WriteErrorInPacket_ReturnsErrorCode)
{
	EXPECT_CALL(stream, write_buffer)
		.WillOnce(Return(4))
		.WillOnce(Invoke(write_failer(errc::broken_pipe)));
	chan.write(buffer(std::vector<uint8_t>(10, 0x01)), errc);
	EXPECT_EQ(errc, errc::make_error_code(errc::broken_pipe));
}

TEST_F(MysqlChannelWriteTest, SyncWrite_SequenceNumberNotZero_RespectsSequenceNumber)
{
	chan.reset_sequence_number(0xab);
	ON_CALL(stream, write_buffer)
		.WillByDefault(Invoke(make_write_handler()));
	chan.write(buffer(std::vector<uint8_t>{0xaa, 0xab, 0xac}), errc);
	verify_buffer({
		0x03, 0x00, 0x00, 0xab, // header
		0xaa, 0xab, 0xac // body
	});
	EXPECT_EQ(errc, error_code());
	EXPECT_EQ(chan.sequence_number(), 0xac);
}

TEST_F(MysqlChannelWriteTest, SyncWrite_SequenceIsFF_WrapsSequenceNumber)
{
	chan.reset_sequence_number(0xff);
	ON_CALL(stream, write_buffer)
		.WillByDefault(Invoke(make_write_handler()));
	chan.write(buffer(std::vector<uint8_t>{0xaa, 0xab, 0xac}), errc);
	verify_buffer({
		0x03, 0x00, 0x00, 0xff, // header
		0xaa, 0xab, 0xac // body
	});
	EXPECT_EQ(errc, error_code());
	EXPECT_EQ(chan.sequence_number(), 0);
}

} // anon namespace


