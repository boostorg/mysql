//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_address.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>

#include <exception>

#include "er_impl_common.hpp"
#include "test_common/netfun_helpers.hpp"
#include "test_integration/streams.hpp"

// Coroutines test async without diagnostics overloads

namespace boost {
namespace mysql {
namespace test {

template <class Base>
class async_coroutine_base : public Base
{
protected:
    using conn_type = typename Base::conn_type;
    using base_type = Base;

    template <class R, class... Args>
    using pmem_t = R (conn_type::*)(Args..., boost::asio::yield_context&&);

    template <class R, class... Args>
    network_result<R> fn_impl(pmem_t<R, Args...> p, Args... args)
    {
        auto res = create_initial_netresult<R>(false);
        boost::asio::spawn(
            this->conn().get_executor(),
            [&](boost::asio::yield_context yield) {
                invoke_and_assign(res, p, this->conn(), std::forward<Args>(args)..., yield[res.err]);
            },
            &rethrow_on_failure
        );
        run_until_completion(this->conn().get_executor());
        return res;
    }

public:
    using Base::Base;
    static constexpr const char* name() noexcept { return "async_coroutines"; }
};

template <class Stream>
class async_coroutine_connection : public async_coroutine_base<connection_base<Stream>>
{
    using base_type = async_coroutine_base<connection_base<Stream>>;

public:
    BOOST_MYSQL_TEST_IMPLEMENT_ASYNC()
};

class any_async_coroutine_connection : public async_coroutine_base<any_connection_base>
{
    using base_type = async_coroutine_base<any_connection_base>;

public:
    BOOST_MYSQL_TEST_IMPLEMENT_ASYNC_ANY()
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

void boost::mysql::test::add_async_coroutines(std::vector<std::reference_wrapper<er_network_variant>>& output)
{
    add_variant<async_coroutine_connection<tcp_socket>>(output);
    add_variant_any<address_type::host_and_port, any_async_coroutine_connection>(output);
}
