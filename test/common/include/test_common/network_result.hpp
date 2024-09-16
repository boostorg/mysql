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
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/assert/source_location.hpp>
#include <boost/core/span.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/list.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
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

    // Use for functions without a diagnostics& parameter
    void validate_no_error_nodiag(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) const
    {
        validate_error(error_code(), create_server_diag("<diagnostics unavailable>"), loc);
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
    value_type get(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        validate_no_error(loc);
        return std::move(value);
    }

    BOOST_ATTRIBUTE_NODISCARD
    value_type get_nodiag(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        validate_no_error_nodiag(loc);
        return std::move(value);
    }
};

// Wraps a network_result and an executor. The result of as_netresult_t.
template <class R>
struct BOOST_ATTRIBUTE_NODISCARD runnable_network_result
{
    struct impl_t
    {
        network_result<R> netres{
            common_server_errc::er_no,
            create_server_diag("network_result_v2 - diagnostics not cleared")
        };
        bool done{false};
    };

    std::unique_ptr<impl_t> impl;

    runnable_network_result() : impl(new impl_t) {}

    network_result<R> run(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        poll_global_context(&impl->done, loc);
        return std::move(impl->netres);
    }

    void validate_no_error(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        std::move(*this).run(loc).validate_no_error(loc);
    }

    void validate_no_error_nodiag(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        std::move(*this).run(loc).validate_no_error_nodiag(loc);
    }

    void validate_error(
        error_code expected_err,
        const diagnostics& expected_diag = {},
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    ) &&
    {
        std::move(*this).run(loc).validate_error(expected_err, expected_diag, loc);
    }

    void validate_error(
        common_server_errc expected_err,
        string_view expected_msg = {},
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    ) &&
    {
        std::move(*this).run(loc).validate_error(expected_err, expected_msg, loc);
    }

    void validate_error(
        client_errc expected_err,
        string_view expected_msg = {},
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    ) &&
    {
        std::move(*this).run(loc).validate_error(expected_err, expected_msg, loc);
    }

    // Use when the exact message isn't known, but some of its contents are
    void validate_error_contains(
        error_code expected_err,
        const std::vector<std::string>& pieces,
        source_location loc = BOOST_MYSQL_CURRENT_LOCATION
    ) &&
    {
        std::move(*this).run(loc).validate_error_contains(expected_err, pieces, loc);
    }

    // Use when you don't care or can't determine the kind of error
    void validate_any_error(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        std::move(*this).run(loc).validate_any_error(loc);
    }

    BOOST_ATTRIBUTE_NODISCARD
    typename network_result<R>::value_type get(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        return std::move(*this).run(loc).get(loc);
    }

    BOOST_ATTRIBUTE_NODISCARD
    typename network_result<R>::value_type get_nodiag(source_location loc = BOOST_MYSQL_CURRENT_LOCATION) &&
    {
        return std::move(*this).run(loc).get_nodiag(loc);
    }
};

struct as_netresult_t
{
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
    typename runnable_network_result<R>::impl_t* target_;
    tracker_executor_result ex_;
    tracker_executor_result immediate_ex_;
    asio::cancellation_slot slot_;
    const diagnostics* diag_ptr;

    void complete(error_code ec) const
    {
        // Check executor. The passed executor must be the top one in all cases.
        // Immediate completions must be dispatched through the immediate executor, too.
        // In all cases, we may encounter a bigger stack because of previous immediate completions.
        const std::array<int, 1> stack_data_regular{{ex_.executor_id}};
        const std::array<int, 2> stack_data_immediate{
            {immediate_ex_.executor_id, ex_.executor_id}
        };

        // Expected top of the executor stack
        boost::span<const int> expected_stack_top = is_initiation_function()
                                                        ? boost::span<const int>(stack_data_immediate)
                                                        : boost::span<const int>(stack_data_regular);

        // Actual top of the executor stack
        auto actual_stack_top = executor_stack().last(
            (std::min)(executor_stack().size(), expected_stack_top.size())
        );

        // Compare
        BOOST_TEST(actual_stack_top == expected_stack_top, boost::test_tools::per_element());

        // Assign error code and diagnostics
        target_->netres.err = ec;
        if (diag_ptr)
            target_->netres.diag = *diag_ptr;
        else
            target_->netres.diag = create_server_diag("<diagnostics unavailable>");

        // Mark the operation as done
        target_->done = true;
    }

public:
    as_netres_handler(
        runnable_network_result<R>& netresult,
        const diagnostics* output_diag,
        asio::cancellation_slot slot
    )
        : target_(netresult.impl.get()),
          ex_(create_tracker_executor(global_context_executor())),
          immediate_ex_(create_tracker_executor(global_context_executor())),
          slot_(slot),
          diag_ptr(output_diag)
    {
    }

    // Executor
    using executor_type = asio::any_io_executor;
    asio::any_io_executor get_executor() const { return ex_.ex; }

    // Immediate executor
    using immediate_executor_type = asio::any_io_executor;
    asio::any_io_executor get_immediate_executor() const { return immediate_ex_.ex; }

    // Cancellation slot
    using cancellation_slot_type = asio::cancellation_slot;
    asio::cancellation_slot get_cancellation_slot() const noexcept { return slot_; }

    void operator()(error_code ec) const { complete(ec); }

    template <class Arg>
    void operator()(error_code ec, Arg&& arg) const
    {
        target_->netres.value = std::forward<Arg>(arg);
        complete(ec);
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
        // Try to find a diagnostics* within the argument list
        using diag_pos = mp11::mp_find<mp11::mp_list<Args...>, mysql::diagnostics*>;
        constexpr bool diag_found = diag_pos::value < sizeof...(Args);

        // Dispatch
        return do_initiate(
            std::integral_constant<bool, diag_found>{},
            diag_pos{},
            std::forward<Initiation>(initiation),
            token,
            std::forward<Args>(args)...
        );
    }

    // Common case optimization: diagnostics* is first
    template <typename Initiation, typename... Args>
    static return_type initiate(
        Initiation&& initiation,
        mysql::test::as_netresult_t token,
        mysql::diagnostics* diag,
        Args&&... args
    )
    {
        return do_initiate_impl(
            std::forward<Initiation>(initiation),
            token,
            diag,
            diag,
            std::forward<Args>(args)...
        );
    }

private:
    // A diagnostics* was found
    template <std::size_t N, typename Initiation, typename... Args>
    static return_type do_initiate(
        std::true_type /* diag_found */,
        mp11::mp_size_t<N> /* diag_pos */,
        Initiation&& initiation,
        mysql::test::as_netresult_t token,
        Args&&... args
    )
    {
        return do_initiate_impl(
            std::forward<Initiation>(initiation),
            token,
            std::get<N>(std::tuple<Args&...>{args...}),
            std::forward<Args>(args)...
        );
    }

    // A diagnostics* was not found
    template <std::size_t N, typename Initiation, typename... Args>
    static return_type do_initiate(
        std::false_type /* diag_found */,
        mp11::mp_size_t<N> /* diag_pos */,
        Initiation&& initiation,
        mysql::test::as_netresult_t token,
        Args&&... args
    )
    {
        return do_initiate_impl(
            std::forward<Initiation>(initiation),
            token,
            nullptr,
            std::forward<Args>(args)...
        );
    }

    template <typename Initiation, typename... Args>
    static return_type do_initiate_impl(
        Initiation&& initiation,
        mysql::test::as_netresult_t token,
        mysql::diagnostics* diag,
        Args&&... args
    )
    {
        // Verify that we correctly set diagnostics in all cases
        if (diag)
            *diag = mysql::test::create_server_diag("Diagnostics not cleared properly");

        // Create the return type
        mysql::test::runnable_network_result<R> netres;

        // Record that we're initiating
        mysql::test::initiation_guard guard;

        // Actually call the initiation function
        std::forward<Initiation>(initiation)(
            mysql::test::test_detail::as_netres_handler<R>(
                netres,
                diag,
                asio::get_associated_cancellation_slot(token)
            ),
            std::forward<Args>(args)...
        );

        return netres;
    }
};

}  // namespace asio
}  // namespace boost

#endif
