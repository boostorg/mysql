//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "network_functions_impl.hpp"
#include <boost/asio/use_awaitable.hpp> // for BOOST_ASIO_HAS_CO_AWAIT
#include <boost/test/unit_test.hpp>
#include "test_common.hpp"

using namespace boost::mysql::test;

// network_result_base
using boost::mysql::test::network_result_base;

static const char* get_message(
    const boost::optional<boost::mysql::error_info>& info
) noexcept
{
    return info ? info->message().c_str() : "<unavailable>";
}

void network_result_base::validate_no_error() const
{
    BOOST_TEST_REQUIRE(err == error_code(),
        "with error_info= " << get_message(info) << ", error_code=" << err.message());
    if (info)
    {
        BOOST_TEST(*info == error_info());
    }
}

void network_result_base::validate_any_error(
    const std::vector<std::string>& expected_msg
) const
{
    BOOST_TEST_REQUIRE(err != error_code(),
        "with error_info= " << get_message(info));
    if (info)
    {
        validate_string_contains(info->message(), expected_msg);
    }
}

void network_result_base::validate_error(
    error_code expected_errc,
    const std::vector<std::string>& expected_msg
) const
{
    BOOST_TEST_REQUIRE(err == expected_errc,
        "with error_info= " << get_message(info));
    if (info)
    {
        validate_string_contains(info->message(), expected_msg);
    }
}

// free functions
template <class Stream>
const std::vector<network_functions<Stream>*>&
boost::mysql::test::all_network_functions()
{
    static std::vector<network_functions<Stream>*> res {
        sync_errc_functions<Stream>(),
        sync_exc_functions<Stream>(),
        async_callback_errinfo_functions<Stream>(),
        async_callback_noerrinfo_functions<Stream>(),
        async_coroutine_errinfo_functions<Stream>(),
        async_coroutine_noerrinfo_functions<Stream>(),
        async_future_errinfo_functions<Stream>(),
        async_future_noerrinfo_functions<Stream>(),
#ifdef BOOST_ASIO_HAS_CO_AWAIT
        async_coroutinecpp20_errinfo_functions<Stream>(),
        async_coroutinecpp20_noerrinfo_functions<Stream>()
#endif
    };
    return res;
}

// We implement a single variant for default completion tokens
// Others do not add any value
template <>
const std::vector<network_functions<tcp_future_socket>*>&
boost::mysql::test::all_network_functions<tcp_future_socket>()
{
    static std::vector<network_functions<tcp_future_socket>*> res {
        async_future_errinfo_functions<tcp_future_socket>(),
        async_future_noerrinfo_functions<tcp_future_socket>(),
    };
    return res;
}

template const std::vector<network_functions<boost::asio::ip::tcp::socket>*>&
boost::mysql::test::all_network_functions();

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
template const std::vector<network_functions<boost::asio::local::stream_protocol::socket>*>&
boost::mysql::test::all_network_functions();
#endif
