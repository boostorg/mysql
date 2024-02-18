//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_address.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <exception>

#include "er_impl_common.hpp"
#include "test_common/netfun_helpers.hpp"
#include "test_integration/streams.hpp"

#ifdef BOOST_ASIO_HAS_CO_AWAIT

// C++20 coroutines test async with diagnostics overloads & deferred tokens

namespace boost {
namespace mysql {
namespace test {

template <class R>
using result_tuple = std::
    conditional_t<std::is_same_v<R, void>, std::tuple<error_code>, std::tuple<error_code, R>>;

using token_t = boost::asio::as_tuple_t<boost::asio::use_awaitable_t<>>;

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

template <class Base>
class async_coroutinecpp20_base : public Base
{
protected:
    using conn_type = typename Base::conn_type;
    using base_type = Base;

    template <class R, class... Args>
    using pmem_t = boost::asio::awaitable<result_tuple<R>> (conn_type::*)(Args..., diagnostics&, token_t&&);

    template <class R, class... Args>
    network_result<R> fn_impl(pmem_t<R, Args...> p, Args... args)
    {
        auto res = create_initial_netresult<R>();

        boost::asio::co_spawn(
            this->conn().get_executor(),
            [&]() -> boost::asio::awaitable<void> {
                // Create the task
                auto aw = (this->conn().*p)(std::forward<Args>(args)..., *res.diag, token_t());

                // Verify that initiation didn't have side effects
                BOOST_TEST(res.diag->server_message() == "diagnostics not cleared properly");

                // Run the task
                auto tup = co_await std::move(aw);
                to_network_result(tup, res);
            },
            // Regular errors are handled via error_code's. This will just
            // propagate unexpected ones.
            &rethrow_on_failure
        );

        run_until_completion(this->conn().get_executor());
        return res;
    }

public:
    using Base::Base;
    static constexpr const char* name() noexcept { return "async_coroutinescpp20"; }
};

template <class Stream>
class async_coroutinecpp20_connection : public async_coroutinecpp20_base<connection_base<Stream>>
{
    using base_type = async_coroutinecpp20_base<connection_base<Stream>>;

public:
    BOOST_MYSQL_TEST_IMPLEMENT_ASYNC()
};

class any_async_coroutinecpp20_connection : public async_coroutinecpp20_base<any_connection_base>
{
    using base_type = async_coroutinecpp20_base<any_connection_base>;

public:
    BOOST_MYSQL_TEST_IMPLEMENT_ASYNC_ANY()
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

void boost::mysql::test::add_async_coroutinescpp20(std::vector<er_network_variant*>& output)
{
    add_variant<async_coroutinecpp20_connection<tcp_ssl_socket>>(output);
    add_variant_any<address_type::host_and_port, any_async_coroutinecpp20_connection>(output);
}

#else

void boost::mysql::test::add_async_coroutinescpp20(std::vector<er_network_variant*>&) {}

#endif
