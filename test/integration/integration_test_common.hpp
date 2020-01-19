#ifndef TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_
#define TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_

#include <gtest/gtest.h>
#include <mysql/connection.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <future>
#include <thread>
#include "test_common.hpp"

namespace mysql
{
namespace test
{

struct IntegTest : testing::Test
{
	mysql::connection_params connection_params {"integ_user", "integ_password", "awesome"};
	boost::asio::io_context ctx;
	mysql::connection<boost::asio::ip::tcp::socket> conn {ctx};
	mysql::error_code errc;
	mysql::error_info info;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard { ctx.get_executor() };
	std::thread runner {[this]{ ctx.run(); } };

	IntegTest()
	{
		try
		{
			boost::asio::ip::tcp::endpoint endpoint (boost::asio::ip::address_v4::loopback(), 3306);
			conn.next_level().connect(endpoint);
			reset_errors();
		}
		catch (...) // prevent terminate without an active exception on connect error
		{
			guard.reset();
			runner.join();
		}
	}

	~IntegTest()
	{
		conn.next_level().close(errc);
		guard.reset();
		runner.join();
	}

	template <typename Callable>
	void validate_async_fail(
		Callable&& initiator,
		error_code expected_errc,
		const std::vector<std::string>& expected_info
	)
	{
		std::promise<std::pair<error_code, error_info>> prom;
		initiator([&prom](error_code errc, error_info info, auto&&...) {
			prom.set_value(std::make_pair(errc, std::move(info)));
		});
		auto [actual_errc, actual_info] = prom.get_future().get();
		EXPECT_EQ(actual_errc, expected_errc);
		validate_error_info(actual_info, expected_info);
	}

	template <typename Callable>
	void validate_async_fail(
		Callable&& initiator,
		Error expected_errc,
		const std::vector<std::string>& expected_info
	)
	{
		validate_async_fail(
			std::forward<Callable>(initiator),
			::mysql::detail::make_error_code(expected_errc),
			expected_info
		);
	}

	template <typename Callable>
	void validate_sync_fail(
		Callable&& cb,
		error_code expected_errc,
		const std::vector<std::string>& expected_msg
	)
	{
		try
		{
			std::forward<Callable>(cb)();
			FAIL() << "Expected error: " << expected_errc.message();
		}
		catch (const boost::system::system_error& err)
		{
			EXPECT_EQ(err.code(), expected_errc);
			validate_string_contains(err.what(), expected_msg);
		}
	}

	template <typename Callable>
	void validate_sync_fail(
		Callable&& cb,
		Error expected_errc,
		const std::vector<std::string>& expected_msg
	)
	{
		validate_sync_fail(std::forward<Callable>(cb), detail::make_error_code(expected_errc), expected_msg);
	}

	void handshake()
	{
		conn.handshake(connection_params);
	}

	void reset_errors()
	{
		// TODO: set errc to something not null to verify we clear stuff
		info.set_message("Previous error message was not cleared correctly");
	}
};

}
}



#endif /* TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_ */
