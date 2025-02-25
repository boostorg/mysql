//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/variant_stream.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/core/span.hpp>
#include <boost/test/tools/detail/per_element_manip.hpp>
#include <boost/test/tools/detail/print_helper.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <ostream>

#include "test_common/io_context_fixture.hpp"
#include "test_common/printing.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;
using asio::cancellation_type_t;
using asio::ip::tcp;
using boost::test_tools::per_element;
using detail::vsconnect_action_type;

static const char* act_type_to_string(vsconnect_action_type act)
{
    switch (act)
    {
    case vsconnect_action_type::resolve: return "vsconnect_action_type::resolve";
    case vsconnect_action_type::connect: return "vsconnect_action_type::connect";
    case vsconnect_action_type::immediate: return "vsconnect_action_type::immediate";
    case vsconnect_action_type::none: return "vsconnect_action_type::none";
    default: return "<unknown vsconnect_action_type>";
    }
}

namespace boost {
namespace mysql {
namespace detail {

std::ostream& operator<<(std::ostream& os, vsconnect_action_type act)
{
    return os << act_type_to_string(act);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

BOOST_TEST_DONT_PRINT_LOG_VALUE(boost::asio::generic::stream_protocol::endpoint)

namespace {

BOOST_AUTO_TEST_SUITE(test_variant_stream)

struct fixture : io_context_fixture
{
    detail::variant_stream_state st{ctx.get_executor(), nullptr};
    any_address addr;

    static std::array<tcp::endpoint, 2> tcp_endpoints()
    {
        return {
            {
             tcp::endpoint(asio::ip::make_address("192.168.10.1"), 1234),
             tcp::endpoint(asio::ip::make_address("fe76::abab:4567:72b4:9876"), 1234),
             }
        };
    }

    static tcp::resolver::results_type make_resolver_results(boost::span<const tcp::endpoint> endpoints)
    {
        return tcp::resolver::results_type::create(endpoints.begin(), endpoints.end(), "my_host", "1234");
    }
};

BOOST_FIXTURE_TEST_CASE(tcp_success, fixture)
{
    // Setup
    addr.emplace_host_and_port("my_host", 1234);
    detail::variant_stream_connect_algo algo{st, addr};

    // Initiate: we should resolve
    auto act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::resolve);
    BOOST_TEST(*act.data.resolve.hostname == "my_host");
    BOOST_TEST(*act.data.resolve.service == "1234");

    // Resolving done: we should connect
    auto endpoints = tcp_endpoints();
    auto r = make_resolver_results(endpoints);
    act = algo.resume(error_code(), &r, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::connect);
    BOOST_TEST(act.data.connect == endpoints, per_element());

    // Connect done: success
    st.sock.open(asio::ip::tcp::v4());  // Simulate a connection - otherwise setting sock options fails
    act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::none);
    BOOST_TEST(act.data.err == error_code());
}

BOOST_FIXTURE_TEST_CASE(tcp_error_resolve, fixture)
{
    // Setup
    addr.emplace_host_and_port("my_host", 1234);
    detail::variant_stream_connect_algo algo{st, addr};

    // Initiate: we should resolve
    auto act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::resolve);
    BOOST_TEST(*act.data.resolve.hostname == "my_host");
    BOOST_TEST(*act.data.resolve.service == "1234");

    // Resolving error: done
    asio::ip::tcp::resolver::results_type r;
    act = algo.resume(asio::error::connection_reset, &r, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::none);
    BOOST_TEST(act.data.err == asio::error::connection_reset);
}

BOOST_FIXTURE_TEST_CASE(tcp_error_connect, fixture)
{
    // Setup
    addr.emplace_host_and_port("my_host", 1234);
    detail::variant_stream_connect_algo algo{st, addr};

    // Initiate: we should resolve
    auto act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::resolve);
    BOOST_TEST(*act.data.resolve.hostname == "my_host");
    BOOST_TEST(*act.data.resolve.service == "1234");

    // Resolving done: we should connect
    auto endpoints = tcp_endpoints();
    auto r = make_resolver_results(endpoints);
    act = algo.resume(error_code(), &r, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::connect);
    BOOST_TEST(act.data.connect == endpoints, per_element());

    // Connect failed: done. No socket option is set
    act = algo.resume(asio::error::connection_reset, nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::none);
    BOOST_TEST(act.data.err == asio::error::connection_reset);
}

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
BOOST_FIXTURE_TEST_CASE(unix_success, fixture)
{
    // Setup
    addr.emplace_unix_path("/my/path");
    detail::variant_stream_connect_algo algo{st, addr};

    // Initiate: we should connect
    const asio::local::stream_protocol::endpoint endpoints[]{"/my/path"};
    auto act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::connect);
    BOOST_TEST(act.data.connect == endpoints, per_element());

    // Connect done: success. No socket option is set
    act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::none);
    BOOST_TEST(act.data.err == error_code());
}

BOOST_FIXTURE_TEST_CASE(unix_error_connect, fixture)
{
    // Setup
    addr.emplace_unix_path("/my/path");
    detail::variant_stream_connect_algo algo{st, addr};

    // Initiate: we should connect
    const asio::local::stream_protocol::endpoint endpoints[]{"/my/path"};
    auto act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::connect);
    BOOST_TEST(act.data.connect == endpoints, per_element());

    // Connect failed: done. No socket option is set
    act = algo.resume(asio::error::network_reset, nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::none);
    BOOST_TEST(act.data.err == asio::error::network_reset);
}
#else
BOOST_FIXTURE_TEST_CASE(unix_unsupported, fixture)
{
    // Setup
    addr.emplace_unix_path("/my/path");
    detail::variant_stream_connect_algo algo{st, addr};

    // Initiate: immediate completion
    auto act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::immediate);

    // Resuming again yields the error
    act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::none);
    BOOST_TEST(act.data.err == asio::error::operation_not_supported);
}
#endif

// Cancellation: we use the cancellation state and error on cancellation
// Only relevant in the TCP case, as UNIX connect is a single operation
// If the cancellation state contains the terminal type, we fail
BOOST_FIXTURE_TEST_CASE(cancellation_contains_terminal, fixture)
{
    struct
    {
        const char* name;
        cancellation_type_t cancellation_state;
    } test_cases[] = {
        {"terminal", cancellation_type_t::terminal},
        {"all",      cancellation_type_t::all     },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            addr.emplace_host_and_port("my_host", 1234);
            detail::variant_stream_connect_algo algo{st, addr};

            // Initiate: we should resolve
            auto act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
            BOOST_TEST(act.type == vsconnect_action_type::resolve);

            // Resolving finished successfully, but the cancellation state is set
            auto endpoints = tcp_endpoints();
            auto r = make_resolver_results(endpoints);
            act = algo.resume(error_code(), &r, tc.cancellation_state);
            BOOST_TEST(act.type == vsconnect_action_type::none);
            BOOST_TEST(act.data.err == asio::error::operation_aborted);
        }
    }
}

// Since we only support terminal cancellation, we ignore other cancellation types
BOOST_FIXTURE_TEST_CASE(cancellation_no_terminal, fixture)
{
    struct
    {
        const char* name;
        cancellation_type_t cancellation_state;
    } test_cases[] = {
        {"partial",       cancellation_type_t::partial                             },
        {"total",         cancellation_type_t::total                               },
        {"partial+total", cancellation_type_t::partial | cancellation_type_t::total},
        {"other",         static_cast<cancellation_type_t>(0x80)                   },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            addr.emplace_host_and_port("my_host", 1234);
            detail::variant_stream_connect_algo algo{st, addr};

            // Initiate: we should resolve
            auto act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
            BOOST_TEST(act.type == vsconnect_action_type::resolve);
            BOOST_TEST(*act.data.resolve.hostname == "my_host");
            BOOST_TEST(*act.data.resolve.service == "1234");

            // Resolving done: we should connect
            auto endpoints = tcp_endpoints();
            auto r = make_resolver_results(endpoints);
            act = algo.resume(error_code(), &r, tc.cancellation_state);
            BOOST_TEST(act.type == vsconnect_action_type::connect);
            BOOST_TEST(act.data.connect == endpoints, per_element());

            // Connect done: success
            // Simulate a connection - otherwise setting sock options fails
            st.sock.open(asio::ip::tcp::v4());
            act = algo.resume(error_code(), nullptr, tc.cancellation_state);
            BOOST_TEST(act.type == vsconnect_action_type::none);
            BOOST_TEST(act.data.err == error_code());
        }
    }
}

// If there is an I/O error and the cancellation state is set, the error wins
BOOST_FIXTURE_TEST_CASE(cancellation_error, fixture)
{
    // Setup
    addr.emplace_host_and_port("my_host", 1234);
    detail::variant_stream_connect_algo algo{st, addr};

    // Initiate: we should resolve
    auto act = algo.resume(error_code(), nullptr, cancellation_type_t::none);
    BOOST_TEST(act.type == vsconnect_action_type::resolve);
    BOOST_TEST(*act.data.resolve.hostname == "my_host");
    BOOST_TEST(*act.data.resolve.service == "1234");

    // Resolving error, and the cancellation state is set
    asio::ip::tcp::resolver::results_type r;
    act = algo.resume(asio::error::connection_reset, &r, cancellation_type_t::terminal);
    BOOST_TEST(act.type == vsconnect_action_type::none);
    BOOST_TEST(act.data.err == asio::error::connection_reset);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace