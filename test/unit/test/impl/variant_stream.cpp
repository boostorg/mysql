//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/variant_stream.hpp>

#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/tools/detail/per_element_manip.hpp>
#include <boost/test/tools/detail/print_helper.hpp>
#include <boost/test/unit_test.hpp>

#include <iterator>
#include <ostream>

#include "test_common/printing.hpp"
#include "test_common/tracker_executor.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;
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

BOOST_TEST_DONT_PRINT_LOG_VALUE(::boost::asio::generic::stream_protocol::endpoint)

namespace {

BOOST_AUTO_TEST_SUITE(test_variant_stream)

/**
connect algo
    TCP success. check naggle
    TCP error in resolve
    TCP error in connect
    UNIX success
    UNIX error in connect
    UNIX error because it's not supported
    UNIX error because it's not supported: integration. sync and async
    could we test that all memory that was allocated was also deallocated?
 */

BOOST_AUTO_TEST_CASE(tcp_success)
{
    detail::variant_stream_state st{global_context_executor(), nullptr};
    any_address addr;
    addr.emplace_host_and_port("my_host", 1234);
    detail::variant_stream_connect_algo algo{st, addr};

    // Initiate: we should resolve
    auto act = algo.resume(error_code(), nullptr);
    BOOST_TEST(act.type == vsconnect_action_type::resolve);
    BOOST_TEST(*act.data.resolve.hostname == "my_host");
    BOOST_TEST(*act.data.resolve.service == "1234");

    // Resolving done
    const asio::ip::tcp::endpoint endpoints[]{
        asio::ip::tcp::endpoint(asio::ip::make_address("192.168.10.1"), 1234),
        // asio::ip::tcp::endpoint(asio::ip::make_address("2001:0000::::::130B"), 1234),
    };
    auto r = asio::ip::tcp::resolver::results_type::create(
        std::begin(endpoints),
        std::end(endpoints),
        "my_host",
        "1234"
    );
    act = algo.resume(error_code(), &r);
    BOOST_TEST(act.type == vsconnect_action_type::connect);
    BOOST_TEST(act.data.connect == endpoints, boost::test_tools::per_element());

    // Connect done
    st.sock.open(asio::ip::tcp::v4());
    act = algo.resume(error_code(), nullptr);
    BOOST_TEST(act.type == vsconnect_action_type::none);
    BOOST_TEST(act.data.err == error_code());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace