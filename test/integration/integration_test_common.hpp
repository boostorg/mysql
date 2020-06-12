//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP

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
#include "get_endpoint.hpp"

namespace boost {
namespace mysql {
namespace test {

/**
 * The base of all integration tests. The fixture constructor creates
 * a connection, an asio io_context and a thread to run it. It also
 * performs the physical connect on the connection's underlying stream.
 *
 * The fixture is template-parameterized by a stream type, so that
 * tests can be run using several stream types. For a new stream type,
 * define a physical_connect() function applicable to that stream.
 */
template <typename Stream>
struct IntegTest : testing::Test
{
    using stream_type = Stream;

    mysql::connection_params params;
    boost::asio::io_context ctx;
    mysql::socket_connection<Stream> conn;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard;
    std::thread runner;

    IntegTest() :
        params("integ_user", "integ_password", "boost_mysql_integtests"),
        conn(ctx.get_executor()),
        guard(ctx.get_executor()),
        runner([this] { ctx.run(); })
    {
    }

    ~IntegTest()
    {
        error_code code;
        error_info info;
        conn.close(code, info);
        guard.reset();
        runner.join();
    }

    void set_credentials(boost::string_view user, boost::string_view password)
    {
        params.set_username(user);
        params.set_password(password);
    }

    void physical_connect()
    {
        conn.next_layer().connect(get_endpoint<Stream>(endpoint_kind::localhost));
    }

    void handshake(ssl_mode m = ssl_mode::require)
    {
        params.set_ssl(ssl_options(m));
        conn.handshake(params);
        validate_ssl(m);
    }

    void connect(ssl_mode m)
    {
        physical_connect();
        handshake(m);
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
        boost::string_view info=""
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

    // Call this in the fixture setup of any test invoking write
    // operations on the database, to prevent race conditions,
    // make the testing environment more stable and speed up the tests
    void start_transaction()
    {
        this->conn.query("START TRANSACTION").fetch_all();
    }

    std::int64_t get_table_size(const std::string& table)
    {
        return this->conn.query("SELECT COUNT(*) FROM " + table)
                .fetch_all().at(0).values().at(0).template get<std::int64_t>();
    }
};

// To be used as test parameter, when a test case should be run
// over all different network_function's.
template <typename Stream>
struct network_testcase : named_tag
{
    network_functions<Stream>* net;

    network_testcase(network_functions<Stream>* funs) :
        net(funs)
    {
    }

    const char* name() const { return net->name(); }

    static std::vector<network_testcase<Stream>> make_all()
    {
        std::vector<network_testcase<Stream>> res;
        for (auto* net: make_all_network_functions<Stream>())
        {
            res.emplace_back(net);
        }
        return res;
    }
};

// To be used as test parameter, when a test should be run over
// all network_function's and ssl_mode's
template <typename Stream>
struct network_testcase_with_ssl
{
    network_functions<Stream>* net;
    ssl_mode ssl;

    network_testcase_with_ssl(network_functions<Stream>* funs, ssl_mode ssl) :
        net(funs),
        ssl(ssl)
    {
    }

    std::string name() const
    {
        return detail::stringize(net->name(), '_', to_string(ssl));
    }

    static std::vector<network_testcase_with_ssl<Stream>> make_all()
    {
        std::vector<network_testcase_with_ssl<Stream>> res;
        for (auto* net: make_all_network_functions<Stream>())
        {
            for (auto ssl: {ssl_mode::require, ssl_mode::disable})
            {
                res.emplace_back(net, ssl);
            }
        }
        return res;
    }
};

/**
 * The base class for tests to be run over multiple stream types
 * and multiple parameters (typically network_function's and ssl_mode's).
 *
 * To define test cases, please do:
 *  1. Select a (possibly template) test parameter (e.g. network_testcase_with_ssl).
 *     The test parameter should have a std::string name() method, and a static
 *     std::vector<ParameterType> make_all() method, that returns a vector with
 *     all test parameters to use.
 *  2. Declare a test fixture, as a struct inheriting from NetworkTest. It should be
 *     a template, parameterized just by the stream type.
 *  3. Define the tests, as methods of the fixture. They should have void() signature.
 *  4. Use BOOST_MYSQL_NETWORK_TEST_SUITE(FixtureName) to instantiate the test suite.
 *     This will create as many suites as stream types officially supported, and call
 *     INSTANTIATE_TEST_SUITE_P. For example, for a test suite MySuite, it may generate
 *     suites MySuiteTCP and MySuiteUNIX, for TCP and UNIX sockets.
 *  5. Use BOOST_MYSQL_NETWORK_TEST(FixtureName, TestName) for each test you defined
 *     as method of the fixture. This will call TEST_P once for each supported stream,
 *     calling the method you defined in the fixture.
 */
template <
    typename Stream,
    typename Param=network_testcase_with_ssl<Stream>
>
struct NetworkTest : public IntegTest<Stream>,
                     public testing::WithParamInterface<Param>
{
};

} // test
} // mysql
} // boost

// Typedefs
#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_HELPER(TestSuiteName, Suffix, Stream) \
    using TestSuiteName##Suffix = TestSuiteName<Stream>;

#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_TCP(TestSuiteName) \
    BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_HELPER(TestSuiteName, TCP, boost::asio::ip::tcp::socket)

#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_TCP_DEFAULT_TOKEN(TestSuiteName) \
    BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_HELPER(TestSuiteName, TCPDefaultToken, boost::mysql::test::tcp_future_socket)

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_UNIX(TestSuiteName) \
        BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_HELPER(TestSuiteName, UNIX, boost::asio::local::stream_protocol::socket)
#else
#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_UNIX(TestSuiteName)
#endif

#define BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS(TestSuiteName) \
    BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_TCP(TestSuiteName) \
    BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_TCP_DEFAULT_TOKEN(TestSuiteName) \
    BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS_UNIX(TestSuiteName)

// Test definition
#define BOOST_MYSQL_NETWORK_TEST_HELPER(TestSuiteName, TestName, Suffix) \
    TEST_P(TestSuiteName##Suffix, TestName) { this->TestName(); }

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
#define BOOST_MYSQL_NETWORK_TEST_UNIX(TestSuiteName, TestName) \
    BOOST_MYSQL_NETWORK_TEST_HELPER(TestSuiteName, TestName, UNIX)
#else
#define BOOST_MYSQL_NETWORK_TEST_UNIX(TestSuiteName, TestName)
#endif

#define BOOST_MYSQL_NETWORK_TEST(TestSuiteName, TestName) \
    BOOST_MYSQL_NETWORK_TEST_HELPER(TestSuiteName, TestName, TCP) \
    BOOST_MYSQL_NETWORK_TEST_HELPER(TestSuiteName, TestName, TCPDefaultToken) \
    BOOST_MYSQL_NETWORK_TEST_UNIX(TestSuiteName, TestName)

// Test suite instantiation
#define BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_HELPER(TestSuiteName, Suffix) \
    INSTANTIATE_TEST_SUITE_P(Default, TestSuiteName##Suffix, testing::ValuesIn( \
        TestSuiteName##Suffix::ParamType::make_all() \
    ), test_name_generator);

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
#define BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_UNIX(TestSuiteName) \
    BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_HELPER(TestSuiteName, UNIX)
#else
#define BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_UNIX(TestSuiteName)
#endif

#define BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE(TestSuiteName) \
    BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_HELPER(TestSuiteName, TCP) \
    BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_HELPER(TestSuiteName, TCPDefaultToken) \
    BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE_UNIX(TestSuiteName)

// Typedefs + Instantiation
#define BOOST_MYSQL_NETWORK_TEST_SUITE(TestSuiteName) \
    BOOST_MYSQL_NETWORK_TEST_SUITE_TYPEDEFS(TestSuiteName) \
    BOOST_MYSQL_INSTANTIATE_NETWORK_TEST_SUITE(TestSuiteName)



#endif /* TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_ */
