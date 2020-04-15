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

inline void physical_connect(tcp_connection& conn)
{
	boost::asio::ip::tcp::endpoint endpoint (boost::asio::ip::address_v4::loopback(), 3306);
	conn.next_layer().connect(endpoint);
}

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
inline void physical_connect(unix_connection& conn)
{
	boost::asio::local::stream_protocol::endpoint endpoint ("/tmp/mysql.sock");
	conn.next_layer().connect(endpoint);
}
#endif

template <typename Stream>
struct IntegTest : testing::Test
{
	using stream_type = Stream;

	mysql::connection_params connection_params {"integ_user", "integ_password", "awesome"};
	boost::asio::io_context ctx;
	mysql::connection<Stream> conn {ctx};
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard { ctx.get_executor() };
	std::thread runner {[this]{ ctx.run(); } };

	IntegTest()
	{
		try
		{
			physical_connect(conn);
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
		const resultset<Stream>& result,
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
		const resultset<Stream>& result,
		const std::string& table
	) const
	{
		validate_2fields_meta(result.fields(), table);
	}

};

template <typename Stream>
struct IntegTestAfterHandshake : IntegTest<Stream>
{
	IntegTestAfterHandshake(ssl_mode m = ssl_mode::require) { this->handshake(m); }
};

template <typename Stream>
struct network_testcase
{
	network_functions<Stream>* net;
	ssl_mode ssl;

	std::string name() const
	{
		return detail::stringize(net->name(), '_', to_string(ssl));
	}
};

template <typename Stream>
inline std::vector<network_testcase<Stream>> make_all_network_testcases()
{
	std::vector<network_testcase<Stream>> res;
	for (auto* net: make_all_network_functions<Stream>())
	{
		for (auto ssl: {ssl_mode::require, ssl_mode::disable})
		{
			res.push_back(network_testcase<Stream>{net, ssl});
		}
	}
	return res;
}

template <typename Stream>
struct NetworkTest : public IntegTestAfterHandshake<Stream>,
					 public testing::WithParamInterface<network_testcase<Stream>>
{
	NetworkTest(): IntegTestAfterHandshake<Stream>(NetworkTest::GetParam().ssl) {}
};

struct universal_name_generator
{
	template <typename T, typename=std::enable_if_t<!std::is_pointer_v<T>>>
	std::string operator()(const T& v) const { return v.name(); }

	template <typename T>
	std::string operator()(const T* v) const { return v->name(); }
};

} // test
} // mysql
} // boost

// Old
#define MYSQL_NETWORK_TEST_SUITE(TestSuiteName) \
	INSTANTIATE_TEST_SUITE_P(Default, TestSuiteName, testing::ValuesIn( \
		make_all_network_testcases<typename TestSuiteName::stream_type>() \
	), [](const auto& param_info) { return param_info.param.name(); })

// Typedefs
#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_HELPER(TestSuiteName, Suffix, Stream) \
	using TestSuiteName##Suffix = TestSuiteName<Stream>;

#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_TCP(TestSuiteName) \
	BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_HELPER(TestSuiteName, TCP, boost::asio::ip::tcp::socket)

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_UNIX(TestSuiteName) \
		BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_HELPER(TestSuiteName, UNIX, boost::asio::local::stream_protocol::socket)
#else
#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_UNIX(TestSuiteName)
#endif

#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS(TestSuiteName) \
	BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_TCP(TestSuiteName) \
	BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_UNIX(TestSuiteName)

// Test definition
#define MYSQL_NETWORK_TEST_HELPER(TestSuiteName, TestName, Suffix) \
	TEST_P(TestSuiteName##Suffix, TestName) { this->TestName(); }

#define MYSQL_NETWORK_TEST_TCP(TestSuiteName, TestName) \
	MYSQL_NETWORK_TEST_HELPER(TestSuiteName, TestName, TCP)

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
#define MYSQL_NETWORK_TEST_UNIX(TestSuiteName, TestName) \
	MYSQL_NETWORK_TEST_HELPER(TestSuiteName, TestName, UNIX)
#else
#define MYSQL_NETWORK_TEST_UNIX(TestSuiteName, TestName)
#endif

#define MYSQL_NETWORK_TEST(TestSuiteName, TestName) \
	MYSQL_NETWORK_TEST_TCP(TestSuiteName, TestName) \
	MYSQL_NETWORK_TEST_UNIX(TestSuiteName, TestName)

// Test suite instantiation
#define BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_HELPER(TestSuiteName, Generator, Suffix) \
	INSTANTIATE_TEST_SUITE_P(Default, TestSuiteName##Suffix, testing::ValuesIn( \
		Generator<typename TestSuiteName##Suffix::stream_type>() \
	), [](const auto& param_info) { \
		return ::boost::mysql::test::universal_name_generator()(param_info.param); \
	});

#define BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_TCP(TestSuiteName, Generator) \
	BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_HELPER(TestSuiteName, Generator, TCP)

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
#define BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_UNIX(TestSuiteName, Generator) \
	BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_HELPER(TestSuiteName, Generator, UNIX)
#else
#define BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_UNIX(TestSuiteName, Generator)
#endif

#define BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE(TestSuiteName, Generator) \
	BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_TCP(TestSuiteName, Generator) \
	BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_UNIX(TestSuiteName, Generator)

// Typedefs + Instantiation
#define MYSQL_NETWORK_TEST_SUITE2(TestSuiteName) \
	BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS(TestSuiteName) \
	BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE(TestSuiteName, make_all_network_testcases)



#endif /* TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_ */
