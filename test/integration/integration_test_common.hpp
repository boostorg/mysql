#ifndef TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_
#define TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_

#include <gtest/gtest.h>
#include <mysql/connection.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <future>

namespace mysql
{
namespace test
{

struct IntegTest : public testing::Test
{
	mysql::connection_params connection_params {
		mysql::collation::utf8_general_ci,
		"root",
		"root",
		"awesome"
	};
	boost::asio::io_context ctx;
	mysql::connection<boost::asio::ip::tcp::socket> conn {ctx};
	mysql::error_code errc;

	IntegTest()
	{
		boost::asio::ip::tcp::endpoint endpoint (boost::asio::ip::address_v4::loopback(), 3306);
		conn.next_level().connect(endpoint);
	}

	~IntegTest()
	{
		conn.next_level().close(errc);
	}

	template <typename T>
	void validate_future_exception(std::future<T>& fut, mysql::error_code expected_errc)
	{
		try
		{
			fut.get();
			FAIL() << "Expected asynchronous operation to fail";
		}
		catch (const boost::system::system_error& exc)
		{
			EXPECT_EQ(exc.code(), expected_errc);
		}
	}

};

}
}



#endif /* TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_ */
