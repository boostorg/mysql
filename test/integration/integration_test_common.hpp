#ifndef TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_
#define TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_

#include <gtest/gtest.h>
#include "boost/mysql/connection.hpp"
#include "boost/mysql/detail/auxiliar/stringize.hpp"
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
			conn.next_layer().connect(endpoint);
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
		conn.next_layer().close(code);
		guard.reset();
		runner.join();
	}

	void set_credentials(std::string_view user, std::string_view password)
	{
		connection_params.set_username(user);
		connection_params.set_password(password);
	}

	void handshake(ssl_mode m = ssl_mode::require)
	{
		connection_params.set_ssl(ssl_options(m));
		conn.handshake(connection_params);
		validate_ssl(m);
	}

	static bool should_use_ssl(ssl_mode m)
	{
		return m == ssl_mode::enable || m == ssl_mode::require;
	}

	// Verifies that we are or are not using SSL, depending on what mode was requested.
	void validate_ssl(ssl_mode m)
	{
		if (should_use_ssl(m))
		{
			// All our test systems MUST support SSL to run these tests
			EXPECT_TRUE(conn.uses_ssl());
		}
		else
		{
			EXPECT_FALSE(conn.uses_ssl());
		}
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
	IntegTestAfterHandshake(ssl_mode m = ssl_mode::require) { handshake(m); }
};

struct network_testcase
{
	network_functions* net;
	ssl_mode ssl;

	std::string name() const
	{
		return detail::stringize(net->name(), '_', to_string(ssl));
	}
};

inline std::vector<network_testcase> make_all_network_testcases()
{
	std::vector<network_testcase> res;
	for (auto* net: all_network_functions)
	{
		for (auto ssl: {ssl_mode::require, ssl_mode::disable})
		{
			res.push_back(network_testcase{net, ssl});
		}
	}
	return res;
}

struct NetworkTest : public IntegTestAfterHandshake,
					 public testing::WithParamInterface<network_testcase>
{
	NetworkTest(): IntegTestAfterHandshake(GetParam().ssl) {}
};

} // test
} // mysql
} // boost

#define MYSQL_NETWORK_TEST_SUITE(TestSuiteName) \
	INSTANTIATE_TEST_SUITE_P(Default, TestSuiteName, testing::ValuesIn( \
		make_all_network_testcases() \
	), [](const auto& param_info) { return param_info.param.name(); })


#endif /* TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_ */
