//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_NETWORK_RESULT_HPP
#define BOOST_MYSQL_TEST_COMMON_INCLUDE_TEST_COMMON_NETWORK_RESULT_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/assert/source_location.hpp>
#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_common/source_location.hpp"
#include "test_common/tracker_executor.hpp"
#include "test_common/validate_string_contains.hpp"

namespace boost {
namespace mysql {
namespace test {

struct no_result
{
};

// network_result: system::result-like type with helper functions
// for tests
struct BOOST_ATTRIBUTE_NODISCARD network_result_base
{
    error_code err;
    diagnostics diag;

    void validate_no_error(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) const
    {
        validate_error(error_code(), diagnostics(), loc);
    }

    void validate_error(
        error_code expected_err,
        const diagnostics& expected_diag = {},
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    ) const
    {
        BOOST_TEST_CONTEXT("Called from " << loc)
        {
            BOOST_TEST(diag == expected_diag);
            BOOST_TEST_REQUIRE(err == expected_err);
        }
    }

    void validate_error(
        common_server_errc expected_err,
        string_view expected_msg = {},
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    )
    {
        validate_error(expected_err, create_server_diag(expected_msg), loc);
    }

    void validate_error(
        client_errc expected_err,
        string_view expected_msg = {},
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    )
    {
        validate_error(expected_err, create_client_diag(expected_msg), loc);
    }

    // Use when the exact message isn't known, but some of its contents are
    void validate_error_contains(
        error_code expected_err,
        const std::vector<std::string>& pieces,
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    )
    {
        BOOST_TEST_CONTEXT("Called from " << loc)
        {
            validate_string_contains(diag.server_message(), pieces);
            BOOST_TEST_REQUIRE(err == expected_err);
        }
    }

    // Use when you don't care or can't determine the kind of error
    void validate_any_error(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) const
    {
        BOOST_TEST_CONTEXT("Called from " << loc) { BOOST_TEST_REQUIRE(err != error_code()); }
    }
};

template <class R>
struct BOOST_ATTRIBUTE_NODISCARD network_result : network_result_base
{
    using value_type = typename std::conditional<std::is_same<R, void>::value, no_result, R>::type;
    value_type value;

    network_result() = default;
    network_result(error_code ec, diagnostics diag, value_type value = {})
        : network_result_base{ec, std::move(diag)}, value(std::move(value))
    {
    }

    BOOST_ATTRIBUTE_NODISCARD
    value_type get(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) const
    {
        validate_no_error(loc);
        return value;
    }
};

// Wraps a network_result and an executor. The result of as_netresult_t.
template <class R>
struct BOOST_ATTRIBUTE_NODISCARD runnable_network_result
{
    struct impl_t
    {
        asio::any_io_executor io_ex;
        network_result<R> netres;
    };

    static void run_until_completion(asio::any_io_executor ex)
    {
        auto& ctx = static_cast<asio::io_context&>(ex.context());
        ctx.restart();
        ctx.run();
    }

    std::unique_ptr<impl_t> impl;

    runnable_network_result(asio::any_io_executor ex)
        : impl(new impl_t{
              std::move(ex),
              {common_server_errc::er_no, create_server_diag("network_result_v2 - diagnostics not cleared")}
    })
    {
    }

    network_result<R> run() &&
    {
        run_until_completion(impl->io_ex);
        return std::move(impl->netres);
    }

    void validate_no_error(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        std::move(*this).run().validate_no_error(loc);
    }

    void validate_error(
        error_code expected_err,
        const diagnostics& expected_diag = {},
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    ) &&
    {
        std::move(*this).run().validate_error(expected_err, expected_diag, loc);
    }

    void validate_error(
        common_server_errc expected_err,
        string_view expected_msg = {},
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    ) &&
    {
        std::move(*this).run().validate_error(expected_err, expected_msg, loc);
    }

    void validate_error(
        client_errc expected_err,
        string_view expected_msg = {},
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    ) &&
    {
        std::move(*this).run().validate_error(expected_err, expected_msg, loc);
    }

    // Use when the exact message isn't known, but some of its contents are
    void validate_error_contains(
        error_code expected_err,
        const std::vector<std::string>& pieces,
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    ) &&
    {
        std::move(*this).run().validate_error_contains(expected_err, pieces, loc);
    }

    // Use when you don't care or can't determine the kind of error
    void validate_any_error(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        std::move(*this).run().validate_any_error(loc);
    }

    BOOST_ATTRIBUTE_NODISCARD
    typename network_result<R>::value_type get(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        return std::move(*this).run().get(loc);
    }
};

struct as_netresult_t
{
    asio::cancellation_slot slot;
};

constexpr as_netresult_t as_netresult{};

namespace test_detail {

template <class Signature>
struct as_netres_sig_to_rtype;

template <>
struct as_netres_sig_to_rtype<void(error_code)>
{
    using type = void;
};

template <class T>
struct as_netres_sig_to_rtype<void(error_code, T)>
{
    using type = T;
};

template <class R>
class as_netres_handler
{
    network_result<R>* target_;
    tracker_executor_result ex_;
    asio::cancellation_slot slot_;
    const diagnostics* diag_ptr;

    void check_executor() const
    {
        BOOST_TEST(!is_initiation_function());
        BOOST_TEST(current_executor_id() == ex_.executor_id);
    }

public:
    as_netres_handler(
        network_result<R>& netresult,
        const diagnostics& output_diag,
        asio::any_io_executor exec,
        asio::cancellation_slot slot
    )
        : target_(&netresult),
          ex_(create_tracker_executor(std::move(exec))),
          slot_(slot),
          diag_ptr(&output_diag)
    {
    }

    // Executor
    using executor_type = asio::any_io_executor;
    asio::any_io_executor get_executor() const { return ex_.ex; }

    // Cancellation slot
    using cancellation_slot_type = asio::cancellation_slot;
    asio::cancellation_slot get_cancellation_slot() const noexcept { return slot_; }

    void operator()(error_code ec) const
    {
        check_executor();
        target_->err = ec;
        target_->diag = *diag_ptr;
    }

    template <class Arg>
    void operator()(error_code ec, Arg&& arg) const
    {
        check_executor();
        target_->err = ec;
        target_->diag = *diag_ptr;
        target_->value = std::forward<Arg>(arg);
    }
};

}  // namespace test_detail

}  // namespace test
}  // namespace mysql
}  // namespace boost

namespace boost {
namespace asio {

template <typename Signature>
class async_result<mysql::test::as_netresult_t, Signature>
{
public:
    using R = typename mysql::test::test_detail::as_netres_sig_to_rtype<Signature>::type;
    using return_type = mysql::test::runnable_network_result<R>;

    template <typename Initiation, typename... Args>
    static return_type initiate(Initiation&& initiation, mysql::test::as_netresult_t token, Args&&... args)
    {
        return do_initiate(std::move(initiation), token.slot, std::move(args)...);
    }

private:
    // initiate() is not allowed to inspect individual arguments
    template <typename Initiation, class IoObjectPtr, typename... Args>
    static return_type do_initiate(
        Initiation&& initiation,
        asio::cancellation_slot slot,
        mysql::diagnostics* diag,
        IoObjectPtr io_obj_ptr,  // may be smart
        Args&&... args
    )
    {
        // Verify that we correctly set diagnostics in all cases
        *diag = mysql::test::create_server_diag("Diagnostics not cleared properly");

        // Create the return type
        mysql::test::runnable_network_result<R> netres(io_obj_ptr->get_executor());

        // Record that we're initiating
        mysql::test::initiation_guard guard;

        // Actually call the initiation function
        std::move(initiation)(
            mysql::test::test_detail::as_netres_handler<R>(
                netres.impl->netres,
                *diag,
                io_obj_ptr->get_executor(),
                slot
            ),
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
