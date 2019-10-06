/*
 * mysql_stream.cpp
 *
 *  Created on: Oct 6, 2019
 *      Author: ruben
 */

#include "mysql/impl/mysql_channel.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/system/system_error.hpp>
#include <boost/asio/buffer.hpp>

using namespace testing;
using namespace mysql;
using namespace mysql::detail;
using namespace boost::asio;

namespace
{

class MockStream
{
public:
	MOCK_METHOD2(read_buffer, std::size_t(boost::asio::mutable_buffer, mysql::error_code&));

	template <typename MutableBufferSequence>
	std::size_t read_some(MutableBufferSequence mb, mysql::error_code& ec)
	{
		if (buffer_size(mb) == 0) return 0;
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
};


struct MysqlChannelTest : public Test
{
	using Channel = MysqlChannel<MockStream>;
	MockStream stream;
	Channel channel {stream};
	std::vector<uint8_t> mem;
	dynamic_vector_buffer<std::uint8_t, std::allocator<uint8_t>> buffer {mem};
	mysql::error_code errc;

	void verify_buffer(const std::vector<uint8_t>& expected)
	{
		std::vector<uint8_t> actual (mem.data(), mem.data() + buffer.size());
		EXPECT_EQ(actual, expected);
	}

	static auto buffer_copier(const std::vector<uint8_t>& buffer)
	{
		return [buffer = move(buffer)](boost::asio::mutable_buffer b, mysql::error_code& ec) {
			memcpy(b.data(), buffer.data(), buffer.size());
			ec.clear();
			return buffer.size();
		};
	}
};

TEST_F(MysqlChannelTest, SyncRead_AllReadsSuccessful_ReadHeaderPopulatesBuffer)
{
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0x03, 0x00, 0x00, 0x00})))
		.WillOnce(Invoke(buffer_copier({0xfe, 0x03, 0x02})));
	channel.read(buffer, errc);
	EXPECT_EQ(errc, error_code());
	verify_buffer({0xfe, 0x03, 0x02});
}

TEST_F(MysqlChannelTest, SyncRead_MoreThan16M_JoinsPackets)
{
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0xff, 0xff, 0xff, 0x00})))
		.WillOnce(Invoke(buffer_copier(std::vector<uint8_t>(0xffffff, 0x20))))
		.WillOnce(Invoke(buffer_copier({0xff, 0xff, 0xff, 0x01})))
		.WillOnce(Invoke(buffer_copier(std::vector<uint8_t>(0xffffff, 0x20))))
		.WillOnce(Invoke(buffer_copier({0x04, 0x00, 0x00, 0x02})))
		.WillOnce(Invoke(buffer_copier({0x20, 0x20, 0x20, 0x20})));
	channel.read(buffer, errc);
	EXPECT_EQ(errc, error_code());
	verify_buffer(std::vector<uint8_t>(0xffffff * 2 + 4, 0x20));
}

TEST_F(MysqlChannelTest, SyncRead_ShortReads_InvokesReadAgain)
{
	EXPECT_CALL(stream, read_buffer)
		.WillOnce(Invoke(buffer_copier({0x04})))
		.WillOnce(Invoke(buffer_copier({     0x00, 0x00, 0x00})))
		.WillOnce(Invoke(buffer_copier({0x01, 0x02})))
		.WillOnce(Invoke(buffer_copier({0x03, 0x04})));
	channel.read(buffer, errc);
	EXPECT_EQ(errc, error_code());
	verify_buffer({0x01, 0x02, 0x03, 0x04});
}


}


