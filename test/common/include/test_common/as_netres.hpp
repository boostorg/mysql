//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_AS_NETRES_HPP
#define BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_AS_NETRES_HPP

#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/connection_impl.hpp>
#include <boost/mysql/detail/engine.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/assert/source_location.hpp>

#include <memory>
#include <utility>
#include <vector>

#include "test_common/create_diagnostics.hpp"
#include "test_common/netfun_helpers.hpp"
#include "test_common/printing.hpp"
#include "test_common/tracker_executor.hpp"
#include "test_common/validate_string_contains.hpp"

namespace boost {
namespace mysql {
namespace test {

struct as_netresult_t
{
};

constexpr as_netresult_t as_netresult{};

// TODO: duplicate
template <class R>
struct BOOST_ATTRIBUTE_NODISCARD network_result_v2
{
    struct no_result_t
    {
    };

    using value_type = typename std::conditional<std::is_same<R, void>::value, no_result_t, R>::type;

    struct impl_t
    {
        asio::any_io_executor io_ex;
        error_code err;
        const diagnostics& diag;
        value_type value;
    };

    std::unique_ptr<impl_t> impl;

    network_result_v2(asio::any_io_executor ex, const diagnostics& output_diag)
        : impl(new impl_t{std::move(ex), common_server_errc::er_no, output_diag, {}})
    {
    }

    void run() { run_until_completion(impl->io_ex); }

    void validate_no_error(source_location loc = BOOST_CURRENT_LOCATION)
    {
        validate_error(error_code(), diagnostics(), loc);
    }

    void validate_error(
        error_code expected_err,
        const diagnostics& expected_diag = {},
        source_location loc = BOOST_CURRENT_LOCATION
    )
    {
        BOOST_TEST_CONTEXT("Called from " << loc)
        {
            run();
            BOOST_TEST(impl->diag == expected_diag);
            BOOST_TEST_REQUIRE(impl->err == expected_err);
        }
    }

    void validate_error(
        error_code expected_err,
        string_view expected_msg,
        source_location loc = BOOST_CURRENT_LOCATION
    )
    {
        validate_error(expected_err, create_server_diag(expected_msg), loc);
    }

    // Use when the exact message isn't known, but some of its contents are
    void validate_error_contains(
        error_code expected_err,
        const std::vector<std::string>& pieces,
        source_location loc = BOOST_CURRENT_LOCATION
    )
    {
        BOOST_TEST_CONTEXT("Called from " << loc)
        {
            run();
            validate_string_contains(impl->diag.server_message(), pieces);
            BOOST_TEST_REQUIRE(impl->err == expected_err);
        }
    }

    value_type get(source_location loc = BOOST_CURRENT_LOCATION)
    {
        validate_no_error(loc);
        return impl->value;
    }

    error_code error() const { return impl->err; }
};

// TODO: rename
template <class Signature>
struct sig_to_network_result_type;

template <>
struct sig_to_network_result_type<void(error_code)>
{
    using type = void;
};

template <class T>
struct sig_to_network_result_type<void(error_code, T)>
{
    using type = T;
};

template <class R>
class as_netres_handler
{
    typename network_result_v2<R>::impl_t* target_;
    tracker_executor_result ex_;

    void check_executor() const
    {
        BOOST_TEST(!is_initiation_function());
        BOOST_TEST(current_executor_id() == ex_.executor_id);
    }

public:
    as_netres_handler(network_result_v2<R>& netresult, asio::any_io_executor exec)
        : target_(netresult.impl.get()), ex_(create_tracker_executor(std::move(exec)))
    {
    }

    using executor_type = asio::any_io_executor;
    asio::any_io_executor get_executor() const { return ex_.ex; }

    void operator()(error_code ec) const
    {
        check_executor();
        target_->err = ec;
    }

    template <class Arg>
    void operator()(error_code ec, Arg&& arg) const
    {
        check_executor();
        target_->err = ec;
        target_->value = std::forward<Arg>(arg);
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

namespace boost {
namespace asio {

template <typename Signature>
class async_result<mysql::test::as_netresult_t, Signature>
{
public:
    using R = typename mysql::test::sig_to_network_result_type<Signature>::type;
    using return_type = mysql::test::network_result_v2<R>;

    template <typename Initiation, typename... Args>
    static return_type initiate(Initiation&& initiation, mysql::test::as_netresult_t, Args&&... args)
    {
        return do_initiate(std::move(initiation), std::move(args)...);
    }

private:
    // initiate() is not allowed to inspect individual arguments
    template <typename Initiation, class IoObjectPtr, typename... Args>
    static return_type do_initiate(
        Initiation&& initiation,
        mysql::diagnostics* diag,
        IoObjectPtr io_obj_ptr,  // may be smart
        Args&&... args
    )
    {
        // Verify that we correctly set diagnostics in all cases
        *diag = mysql::test::create_server_diag("Diagnostics not cleared properly");

        // Create the return type
        mysql::test::network_result_v2<R> netres(io_obj_ptr->get_executor(), *diag);

        // Record that we're initiating
        mysql::test::initiation_guard guard;

        // Actually call the initiation function
        std::move(initiation)(
            mysql::test::as_netres_handler<R>(netres, io_obj_ptr->get_executor()),
            diag,
            std::move(io_obj_ptr),
            std::move(args)...
        );

        return netres;
    }
};

}  // namespace asio
}  // namespace boost

#endif
