//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP

#include "boost/mysql/connection.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/test/unit_test.hpp>
#include <thread>
#include "test_common.hpp"
#include "metadata_validator.hpp"
#include "network_functions.hpp"
#include "get_endpoint.hpp"
#include "network_test.hpp"

namespace boost {
namespace mysql {
namespace test {

// Verifies that we are or are not using SSL, depending on what mode was requested.
template <class Stream>
void validate_ssl(const connection<Stream>& conn, ssl_mode m)
{
    // All our test systems MUST support SSL to run these tests
    bool should_use_ssl =
        m == ssl_mode::enable || m == ssl_mode::require;
    BOOST_TEST(conn.uses_ssl() == should_use_ssl);
}

struct use_external_ctx_t {};
constexpr use_external_ctx_t use_external_ctx {};

/**
 * Base fixture to use in integration tests. The fixture constructor creates
 * a connection, an asio io_context and a thread to run it.
 * The fixture is template-parameterized by a stream type, as required
 * by BOOST_MYSQL_NETWORK_TEST.
 */
template <class Stream>
struct network_fixture
{
    using stream_type = Stream;

    boost::asio::ssl::context external_ctx {boost::asio::ssl::context::tls_client}; // for external ctx tests
    connection_params params;
    boost::asio::io_context ctx;
    socket_connection<Stream> conn;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard;
    std::thread runner;

    network_fixture() :
        params("integ_user", "integ_password", "boost_mysql_integtests"),
        conn(ctx.get_executor()),
        guard(ctx.get_executor()),
        runner([this] { ctx.run(); })
    {
    }

    network_fixture(use_external_ctx_t) :
        params("integ_user", "integ_password", "boost_mysql_integtests"),
        conn(external_ctx, ctx.get_executor()),
        guard(ctx.get_executor()),
        runner([this] { ctx.run(); })
    {
    }

    ~network_fixture()
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
        params.set_ssl(m);
        conn.handshake(params);
        validate_ssl(conn, m);
    }

    void connect(ssl_mode m)
    {
        physical_connect();
        handshake(m);
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
        this->conn.query("START TRANSACTION").read_all();
    }

    std::int64_t get_table_size(const std::string& table)
    {
        return this->conn.query("SELECT COUNT(*) FROM " + table)
                .read_all().at(0).values().at(0).template get<std::int64_t>();
    }
};

// To be used as sample in data driven tests, when a test case should be run
// over all different network_function's.
template <class Stream>
struct network_sample
{
    network_functions<Stream>* net;

    network_sample(network_functions<Stream>* funs) :
        net(funs)
    {
    }

    void set_test_attributes(boost::unit_test::test_case& test) const
    {
        test.add_label(net->name());
    }
};

template <class Stream>
std::ostream& operator<<(std::ostream& os, const network_sample<Stream>& value)
{
    return os << value.net->name();
}

// Data generator for network_sample
struct network_gen
{
    template <class Stream>
    static std::vector<network_sample<Stream>> make_all()
    {
        std::vector<network_sample<Stream>> res;
        for (auto* net: all_network_functions<Stream>())
        {
            res.emplace_back(net);
        }
        return res;
    }

    template <class Stream>
    static const std::vector<network_sample<Stream>>& generate()
    {
        static std::vector<network_sample<Stream>> res = make_all<Stream>();
        return res;
    }
};

// To be used as sample in data driven tests,
// when a test should be run over
// all network_function's and ssl_mode's
template <class Stream>
struct network_ssl_sample
{
    network_functions<Stream>* net;
    ssl_mode ssl;

    network_ssl_sample(network_functions<Stream>* funs, ssl_mode ssl) :
        net(funs),
        ssl(ssl)
    {
    }

    void set_test_attributes(boost::unit_test::test_case& test) const
    {
        test.add_label(net->name());
        test.add_label(to_string(ssl));
    }
};

template <class Stream>
std::ostream& operator<<(std::ostream& os, const network_ssl_sample<Stream>& value)
{
    return os << value.net->name() << '_' << to_string(value.ssl);
}

// Data generator for network_ssl_sample
struct network_ssl_gen
{
    template <class Stream>
    static std::vector<network_ssl_sample<Stream>> make_all()
    {
        std::vector<network_ssl_sample<Stream>> res;
        for (auto* net: all_network_functions<Stream>())
        {
            for (auto ssl: {ssl_mode::require, ssl_mode::disable})
            {
                res.emplace_back(net, ssl);
            }
        }
        return res;
    }

    template <class Stream>
    static const std::vector<network_ssl_sample<Stream>>& generate()
    {
        static auto res = make_all<Stream>();
        return res;
    }
};

} // test
} // mysql
} // boost

#endif /* TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_ */
