/*
 * handshake.cpp
 *
 *  Created on: Oct 27, 2019
 *      Author: ruben
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mysql/impl/handshake.hpp"

using namespace testing;
using namespace mysql;
using namespace detail;

namespace
{

class MockChannel
{
public:
	MOCK_METHOD2(read_impl, void(std::vector<std::uint8_t>&, error_code&));
	MOCK_METHOD2(write, void(boost::asio::const_buffer, error_code&));

	template <typename Allocator>
	void read(bytestring<Allocator>& buffer, error_code& errc)
	{
		read_impl(buffer, errc);
	}
};

}

