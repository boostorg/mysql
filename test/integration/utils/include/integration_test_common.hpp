//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_INTEGRATION_TEST_COMMON_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_INTEGRATION_TEST_COMMON_HPP

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/test/unit_test.hpp>
#include <thread>
#include <boost/mysql/handshake_params.hpp>
#include "test_common.hpp"
#include "metadata_validator.hpp"
#include "network_test.hpp"
#include "er_connection.hpp"
#include "er_network_variant.hpp"

namespace boost {
namespace mysql {
namespace test {

struct network_fixture_base
{
    connection_params params {"integ_user", "integ_password", "boost_mysql_integtests"};
    boost::asio::io_context ctx;
    boost::asio::ssl::context ssl_ctx {boost::asio::ssl::context::tls_client};
};

struct network_fixture : network_fixture_base
{
    er_network_variant* var {};
    er_connection_ptr conn;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard;
    std::thread runner;

    network_fixture() :
        guard(ctx.get_executor()),
        runner([this] { ctx.run(); })
    {
    }

    ~network_fixture()
    {
        if (conn)
        {
            conn->sync_close();
        }
        guard.reset();
        runner.join();
    }

    void setup(er_network_variant* variant)
    {
        var = variant;
        conn = var->create(ctx.get_executor(), ssl_ctx);
    }

    void setup_and_connect(er_network_variant* variant, ssl_mode m = ssl_mode::require)
    {
        setup(variant);
        connect(m);
    }

    void set_credentials(boost::string_view user, boost::string_view password)
    {
        params.set_username(user);
        params.set_password(password);
    }

    // Verifies that we are or are not using SSL, depending on whether the stream supports it or not
    void validate_ssl(ssl_mode m = ssl_mode::require)
    {
        bool expected = (m == ssl_mode::require || m == ssl_mode::enable) && var->supports_ssl();
        BOOST_TEST(conn->uses_ssl() == expected);
    }

    void handshake(ssl_mode m = ssl_mode::require)
    {
        assert(conn);
        params.set_ssl(m);
        conn->handshake(params).validate_no_error();
        validate_ssl(m);
    }

    void connect(ssl_mode m = ssl_mode::require)
    {
        assert(conn);
        params.set_ssl(m);
        conn->connect(er_endpoint::valid, params).validate_no_error();
        validate_ssl(m);
    }

    void validate_2fields_meta(
        const std::vector<metadata>& fields,
        const std::string& table
    ) const
    {
        validate_meta(fields, {
            meta_validator(table, "id", field_type::int_),
            meta_validator(table, "field_varchar", field_type::varchar)
        });
    }

    void validate_2fields_meta(
        const er_resultset& result,
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
        conn->query("START TRANSACTION").get()->read_all().get();
    }

    std::int64_t get_table_size(const std::string& table)
    {
        return conn->query("SELECT COUNT(*) FROM " + table).get()
                ->read_all().get().at(0).values().at(0).template get<std::int64_t>();
    }
};

// To be used as sample in data driven tests, when a test case should be run
// over all different network_function's.
struct network_sample
{
    er_network_variant* net;

    network_sample(er_network_variant* var) :
        net(var)
    {
    }

    void set_test_attributes(boost::unit_test::test_case& test) const
    {
        if (net->supports_ssl())
        {
            test.add_label("ssl");
        }
        test.add_label(net->stream_name());
        test.add_label(net->variant_name());
    }
};

inline std::ostream& operator<<(std::ostream& os, const network_sample& value)
{
    return os << value.net->stream_name() << "_" << value.net->variant_name();
}

struct all_variants_gen
{
    std::vector<network_sample> make_all() const
    {
        std::vector<network_sample> res;
        for (auto* net: all_variants())
        {
            res.emplace_back(net);
        }
        return res;
    }

    const std::vector<network_sample>& operator()() const
    {
        static auto res = make_all();
        return res;
    }
};

struct ssl_only_gen
{
    std::vector<network_sample> make_all() const
    {
        std::vector<network_sample> res;
        for (auto* net: ssl_variants())
        {
            res.emplace_back(net);
        }
        return res;
    }

    const std::vector<network_sample>& operator()() const
    {
        static auto res = make_all();
        return res;
    }
};

struct non_ssl_only_gen
{
    std::vector<network_sample> make_all() const
    {
        std::vector<network_sample> res;
        for (auto* net: non_ssl_variants())
        {
            res.emplace_back(net);
        }
        return res;
    }

    const std::vector<network_sample>& operator()() const
    {
        static auto res = make_all();
        return res;
    }
};


} // test
} // mysql
} // boost

#endif /* TEST_INTEGRATION_INTEGRATION_TEST_COMMON_HPP_ */
