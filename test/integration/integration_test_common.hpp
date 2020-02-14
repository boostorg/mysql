#ifndef TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_
#define TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_

#include <gtest/gtest.h>
#include <mysql/connection.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <future>
#include <thread>
#include <functional>
#include "test_common.hpp"
#include "metadata_validator.hpp"
#include "network_functions.hpp"

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
			throw;
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

	void validate_sync_fail(
		error_code expected_errc,
		const std::vector<std::string>& expected_msg
	)
	{
		EXPECT_EQ(errc, expected_errc);
		validate_string_contains(info.message(), expected_msg);
	}

	void validate_sync_fail(
		Error expected_errc,
		const std::vector<std::string>& expected_msg
	)
	{
		validate_sync_fail(detail::make_error_code(expected_errc), expected_msg);
	}

	void handshake()
	{
		conn.handshake(connection_params);
	}

	// To ensure we clear things passed by lvalue reference
	void reset_errors()
	{
		errc = detail::make_error_code(mysql::Error::no);
		info.set_message("Previous error message was not cleared correctly");
	}

	void validate_no_error()
	{
		ASSERT_EQ(errc, error_code());
		EXPECT_EQ(info, error_info());
	}

	using resultset_type = mysql::resultset<boost::asio::ip::tcp::socket>;

	void validate_eof(
		const resultset_type& result,
		int affected_rows=0,
		int warnings=0,
		int last_insert=0,
		std::string_view info=""
	)
	{
		EXPECT_TRUE(result.valid());
		EXPECT_TRUE(result.complete());
		EXPECT_EQ(result.affected_rows(), affected_rows);
		EXPECT_EQ(result.warning_count(), warnings);
		EXPECT_EQ(result.last_insert_id(), last_insert);
		EXPECT_EQ(result.info(), info);
	}

	void validate_2fields_meta(
		const std::vector<field_metadata>& fields,
		const std::string& table
	) const
	{
		validate_meta(fields, {
			meta_validator(table, "id", field_type::int_),
			meta_validator(table, "field_varchar", field_type::varchar)
		});
	}

	void validate_2fields_meta(
		const resultset_type& result,
		const std::string& table
	) const
	{
		validate_2fields_meta(result.fields(), table);
	}

};

struct IntegTestAfterHandshake : IntegTest
{
	IntegTestAfterHandshake() { handshake(); }
};

template <typename BaseType = IntegTestAfterHandshake>
struct NetworkTest : public BaseType,
					 public testing::WithParamInterface<network_functions*>
{
};

}
}

#endif /* TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_ */
