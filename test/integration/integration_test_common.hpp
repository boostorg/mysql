#ifndef TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_
#define TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_

#include <gtest/gtest.h>
#include "boost/mysql/connection.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <future>
#include <thread>
#include <functional>
#include "test_common.hpp"
#include "metadata_validator.hpp"
#include "network_functions.hpp"

namespace boost {
namespace mysql {
namespace test {

struct IntegTest : testing::Test
{
	mysql::connection_params connection_params {"integ_user", "integ_password", "awesome"};
	boost::asio::io_context ctx;
	mysql::connection<boost::asio::ip::tcp::socket> conn {ctx};
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard { ctx.get_executor() };
	std::thread runner {[this]{ ctx.run(); } };

	IntegTest()
	{
		try
		{
			boost::asio::ip::tcp::endpoint endpoint (boost::asio::ip::address_v4::loopback(), 3306);
			conn.next_level().connect(endpoint);
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
		error_code code;
		conn.next_level().close(code);
		guard.reset();
		runner.join();
	}

	void handshake()
	{
		conn.handshake(connection_params);
	}

	void validate_eof(
		const tcp_resultset& result,
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
		const tcp_resultset& result,
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

} // test
} // mysql
} // boost

#endif /* TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_ */
