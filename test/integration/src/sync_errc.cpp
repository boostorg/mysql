//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_address.hpp>

#include "er_impl_common.hpp"
#include "test_common/netfun_helpers.hpp"
#include "test_integration/streams.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;

// MSVC complains about passing empty tokens, which is valid C++
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable : 4003)
#endif

namespace boost {
namespace mysql {
namespace test {

template <class Base>
class sync_errc_base : public Base
{
protected:
    using conn_type = typename Base::conn_type;
    using base_type = Base;

    // workaround for gcc5
    template <class R, class... Args>
    struct pmem
    {
        using type = R (conn_type::*)(Args..., error_code&, diagnostics&);
    };

    template <class R, class... Args>
    network_result<R> fn_impl(typename pmem<R, Args...>::type p, Args... args)
    {
        auto res = create_initial_netresult<R>();
        invoke_and_assign(res, p, this->conn(), std::forward<Args>(args)..., res.err, *res.diag);
        return res;
    }

public:
    using Base::Base;
    static constexpr const char* name() noexcept { return "sync_errc"; }
};

template <class Stream>
class sync_errc_connection : public sync_errc_base<connection_base<Stream>>
{
    using base_type = sync_errc_base<connection_base<Stream>>;

public:
    BOOST_MYSQL_TEST_IMPLEMENT_SYNC()
};

class any_sync_errc_connection final : public sync_errc_base<any_connection_base>
{
    using base_type = sync_errc_base<any_connection_base>;

public:
    BOOST_MYSQL_TEST_IMPLEMENT_SYNC_ANY()
};

template <class Stream>
void add_sync_errc_variant(std::vector<er_network_variant*>& output)
{
    add_variant<sync_errc_connection<Stream>>(output);
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

void boost::mysql::test::add_sync_errc(std::vector<er_network_variant*>& output)
{
    // Verify that all streams work
    add_sync_errc_variant<tcp_socket>(output);
    add_sync_errc_variant<tcp_ssl_socket>(output);
    add_variant_any<address_type::host_and_port, any_sync_errc_connection>(output);
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
    add_sync_errc_variant<unix_socket>(output);
    add_sync_errc_variant<unix_ssl_socket>(output);
    add_variant_any<address_type::unix_path, any_sync_errc_connection>(output);
#endif
}

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif