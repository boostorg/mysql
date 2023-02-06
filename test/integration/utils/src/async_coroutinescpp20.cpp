//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/server_diagnostics.hpp>
#include <boost/mysql/tcp_ssl.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <tuple>
#include <type_traits>

#include "er_impl_common.hpp"
#include "netfun_helpers.hpp"
#include "network_result.hpp"
#include "streams.hpp"

#ifdef BOOST_ASIO_HAS_CO_AWAIT

using namespace boost::mysql::test;
using boost::asio::any_io_executor;
using boost::asio::awaitable;
using boost::mysql::error_code;
using boost::mysql::server_diagnostics;

namespace {

// C++20 coroutines also test default completion tokens.
// Default completion tokens can't use the netmaker concept, because
// they rely on default function args, which can't be represented with function pointers

template <class R>
using result_tuple = std::
    conditional_t<std::is_same_v<R, void>, std::tuple<error_code>, std::tuple<error_code, R>>;

template <class R>
void to_network_result(const std::tuple<error_code, R>& tup, network_result<R>& netresult)
{
    netresult.err = std::get<0>(tup);
    netresult.value = std::get<1>(tup);
}

void to_network_result(const std::tuple<error_code>& tup, network_result<void>& netresult)
{
    netresult.err = std::get<0>(tup);
}

void verify_message(const network_result_base& r)
{
    BOOST_TEST(r.diag->message() == "server_diagnostics not cleared properly");
}

template <class R>
using awaitable_fn = std::function<awaitable<result_tuple<R>>(server_diagnostics&)>;

template <class R>
network_result<R> impl_aux(any_io_executor ex, awaitable_fn<R> fn)
{
    auto res = create_initial_netresult<R>();

    boost::asio::co_spawn(
        ex,
        [&]() -> boost::asio::awaitable<void> {
            // Create the task
            auto aw = fn(*res.diag);

            // Verify that initiation didn't have side effects
            verify_message(res);

            // Run the task
            auto tup = co_await std::move(aw);
            to_network_result(tup, res);
        },
        // Regular errors are handled via error_code's. This will just
        // propagate unexpected ones.
        &rethrow_on_failure
    );

    run_until_completion(ex);
    return res;
}

template <class Obj, class Callable>
auto impl(Obj& obj, Callable&& fn)
{
    using tup_type = typename decltype(fn(std::declval<server_diagnostics&>()))::value_type;
    if constexpr (std::tuple_size_v<tup_type> == 1)
    {
        return impl_aux<void>(obj.get_executor(), fn);
    }
    else
    {
        return impl_aux<std::tuple_element_t<1, tup_type>>(obj.get_executor(), fn);
    }
}

using token_t = boost::asio::as_tuple_t<boost::asio::use_awaitable_t<>>;
using stream_type = token_t::as_default_on_t<boost::mysql::tcp_ssl_connection>::stream_type;
using conn_type = boost::mysql::connection<stream_type>;
using stmt_type = boost::mysql::statement<stream_type>;

#define BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(obj_type, fn_name)             \
    [](obj_type& obj, auto&&... args) {                                      \
        return impl(obj, [&](server_diagnostics& diag) {                     \
            return obj.fn_name(std::forward<decltype(args)>(args)..., diag); \
        });                                                                  \
    }

function_table<stream_type> create_table()
{
    return {
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(stmt_type, async_execute),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(stmt_type, async_start_execution),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(stmt_type, async_start_execution),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(stmt_type, async_close),

        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_connect),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_handshake),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_query),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_start_query),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_prepare_statement),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_read_one_row),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_read_some_rows),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_ping),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_quit),
        BOOST_MYSQL_DEFAULT_TOKEN_TABLE_ENTRY(conn_type, async_close),
    };
}

}  // namespace

namespace boost {
namespace mysql {
namespace test {

template <>
constexpr const char* get_stream_name<stream_type>()
{
    return "tcp_ssl_default_tokens";
}

template <>
constexpr bool supports_ssl<stream_type>()
{
    return true;
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

void boost::mysql::test::add_async_coroutinescpp20(std::vector<er_network_variant*>& output)
{
    static er_network_variant_impl<stream_type> net_variant{create_table(), "async_coroutinescpp20"};
    output.push_back(&net_variant);
}

#else

void boost::mysql::test::add_async_coroutinescpp20(std::vector<er_network_variant*>&) {}

#endif
