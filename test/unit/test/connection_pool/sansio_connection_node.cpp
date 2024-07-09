//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/connection_pool/sansio_connection_node.hpp>

#include <boost/test/unit_test.hpp>

#include <cstddef>

#include "test_unit/printing.hpp"

using namespace boost::mysql::detail;
using boost::mysql::client_errc;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_sansio_connection_node)

enum hooks_t : int
{
    enter_idle = 1,
    exit_idle = 2,
    enter_pending = 4,
    exit_pending = 8,
};

struct mock_node : public sansio_connection_node<mock_node>
{
    using sansio_connection_node<mock_node>::sansio_connection_node;

    std::size_t num_entering_idle{};
    std::size_t num_exiting_idle{};
    std::size_t num_entering_pending{};
    std::size_t num_exiting_pending{};

    void entering_idle() { ++num_entering_idle; }
    void exiting_idle() { ++num_exiting_idle; }
    void entering_pending() { ++num_entering_pending; }
    void exiting_pending() { ++num_exiting_pending; }

    void clear_hooks()
    {
        num_entering_idle = 0;
        num_exiting_idle = 0;
        num_entering_pending = 0;
        num_exiting_pending = 0;
    }

    void check(connection_status expected_status, int hooks)
    {
        BOOST_TEST(status() == expected_status);
        BOOST_TEST(num_entering_idle == (hooks & enter_idle ? 1u : 0u));
        BOOST_TEST(num_exiting_idle == (hooks & exit_idle ? 1u : 0u));
        BOOST_TEST(num_entering_pending == (hooks & enter_pending ? 1u : 0u));
        BOOST_TEST(num_exiting_pending == (hooks & exit_pending ? 1u : 0u));
        clear_hooks();
    }
};

// Success state transitions
BOOST_AUTO_TEST_CASE(normal_lifecyle)
{
    // Initial
    mock_node nod;
    nod.check(connection_status::initial, 0);

    // First resume yields connect
    auto act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::connect);
    nod.check(connection_status::connect_in_progress, enter_pending);

    // Connect success
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check(connection_status::idle, exit_pending | enter_idle);

    // Connection taken by user
    nod.mark_as_in_use();
    nod.check(connection_status::in_use, exit_idle);

    // Connection returned by user
    act = nod.resume(error_code(), collection_state::needs_collect_with_reset);
    BOOST_TEST(act == next_connection_action::reset);
    nod.check(connection_status::reset_in_progress, enter_pending);

    // Reset successful
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check(connection_status::idle, exit_pending | enter_idle);

    // Terminate
    nod.cancel();
    act = nod.resume(error_code(), collection_state::needs_collect_with_reset);
    BOOST_TEST(act == next_connection_action::none);
}

BOOST_AUTO_TEST_CASE(collect_without_reset)
{
    // Initial: connection idle
    mock_node nod(connection_status::idle);

    // Connection taken by the user
    nod.mark_as_in_use();
    nod.check(connection_status::in_use, exit_idle);

    // Connection returned without reset
    auto act = nod.resume(error_code(), collection_state::needs_collect);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check(connection_status::idle, enter_idle);
}

BOOST_AUTO_TEST_CASE(collect_still_in_use)
{
    // Initial: connection idle
    mock_node nod(connection_status::idle);

    // Connection taken by the user
    nod.mark_as_in_use();
    nod.check(connection_status::in_use, exit_idle);

    // Idle wait finishes but the connection is still in use
    auto act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check(connection_status::in_use, 0);
}

BOOST_AUTO_TEST_CASE(ping_success)
{
    // Connection idle
    mock_node nod(connection_status::idle);

    // Time elapses and the connection is not taken by the user
    auto act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::ping);
    nod.check(connection_status::ping_in_progress, exit_idle | enter_pending);

    // Ping succeeds, we're idle again
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check(connection_status::idle, exit_pending | enter_idle);
}

// Error state transitions
BOOST_AUTO_TEST_CASE(connect_error)
{
    // Connection trying to connect
    mock_node nod(connection_status::connect_in_progress);

    // Fail connecting
    auto act = nod.resume(client_errc::timeout, collection_state::none);
    BOOST_TEST(act == next_connection_action::sleep_connect_failed);
    nod.check(connection_status::sleep_connect_failed_in_progress, 0);

    // Sleep done
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::connect);
    nod.check(connection_status::connect_in_progress, 0);

    // Connect success
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check(connection_status::idle, exit_pending | enter_idle);
}

BOOST_AUTO_TEST_CASE(ping_error)
{
    // Connection idle
    mock_node nod(connection_status::idle);

    // Time elapses and the connection is not taken by the user
    auto act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::ping);
    nod.check(connection_status::ping_in_progress, exit_idle | enter_pending);

    // Ping fails
    act = nod.resume(client_errc::cancelled, collection_state::none);
    BOOST_TEST(act == next_connection_action::connect);
    nod.check(connection_status::connect_in_progress, 0);

    // Connect succeeds, we're idle again
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check(connection_status::idle, exit_pending | enter_idle);
}

BOOST_AUTO_TEST_CASE(reset_error)
{
    // Connection in use
    mock_node nod(connection_status::in_use);

    // Returned by the user
    auto act = nod.resume(error_code(), collection_state::needs_collect_with_reset);
    BOOST_TEST(act == next_connection_action::reset);
    nod.check(connection_status::reset_in_progress, enter_pending);

    // Reset fails
    act = nod.resume(client_errc::timeout, collection_state::none);
    BOOST_TEST(act == next_connection_action::connect);
    nod.check(connection_status::connect_in_progress, 0);

    // Connect succeeds, we're idle again
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check(connection_status::idle, exit_pending | enter_idle);
}

BOOST_AUTO_TEST_CASE(sleep_between_retries_fail)
{
    // Note: this is an edge case. This op should not fail unless
    // canceled, and this would come with a cancel() call

    // Connection trying to connect
    mock_node nod(connection_status::connect_in_progress);

    // Fail connecting
    auto act = nod.resume(client_errc::timeout, collection_state::none);
    BOOST_TEST(act == next_connection_action::sleep_connect_failed);
    nod.check(connection_status::sleep_connect_failed_in_progress, 0);

    // Sleep reports an error. It will get ignored
    act = nod.resume(client_errc::cancelled, collection_state::none);
    BOOST_TEST(act == next_connection_action::connect);
    nod.check(connection_status::connect_in_progress, 0);
}

BOOST_AUTO_TEST_CASE(idle_wait_fail)
{
    // Note: this is an edge case. This op should not fail unless
    // canceled, and this would come with a cancel() call

    // Connection idle
    mock_node nod(connection_status::idle);

    // Idle wait failed. Error gets ignored
    auto act = nod.resume(client_errc::cancelled, collection_state::none);
    BOOST_TEST(act == next_connection_action::ping);
    nod.check(connection_status::ping_in_progress, exit_idle | enter_pending);
}

BOOST_AUTO_TEST_CASE(idle_wait_fail_in_use)
{
    // Note: this is an edge case. This op should not fail unless
    // canceled, and this would come with a cancel() call

    // Connection in use
    mock_node nod(connection_status::in_use);

    // Idle wait failed. Error gets ignored
    auto act = nod.resume(client_errc::cancelled, collection_state::needs_collect_with_reset);
    BOOST_TEST(act == next_connection_action::reset);
    nod.check(connection_status::reset_in_progress, enter_pending);
}

// Cancellations
BOOST_AUTO_TEST_CASE(cancel)
{
    struct
    {
        connection_status initial_status;
        int hooks;
    } test_cases[] = {
        {connection_status::connect_in_progress,              exit_pending},
        {connection_status::sleep_connect_failed_in_progress, exit_pending},
        {connection_status::idle,                             exit_idle   },
        {connection_status::in_use,                           0           },
        {connection_status::ping_in_progress,                 exit_pending},
        {connection_status::reset_in_progress,                exit_pending},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.initial_status)
        {
            mock_node nod(tc.initial_status);

            // Cancel
            nod.cancel();

            // Next action will always return none
            auto act = nod.resume(client_errc::cancelled, collection_state::none);
            BOOST_TEST(act == next_connection_action::none);
            nod.check(connection_status::terminated, tc.hooks);

            // Cancel again does nothing
            nod.cancel();
            act = nod.resume(client_errc::cancelled, collection_state::none);
            BOOST_TEST(act == next_connection_action::none);
            nod.check(connection_status::terminated, 0);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
