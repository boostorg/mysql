//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/connection_pool/sansio_connection_node.hpp>

#include <boost/test/unit_test.hpp>

#include <cstddef>

#include "pool_printing.hpp"

using namespace boost::mysql::detail;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_sansio_connection_node)

struct mock_node : public sansio_connection_node<mock_node>
{
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

    void check_hooks(
        std::size_t enter_idle,
        std::size_t exit_idle,
        std::size_t enter_pending,
        std::size_t exit_pending
    )
    {
        BOOST_TEST(num_entering_idle == enter_idle);
        BOOST_TEST(num_exiting_idle == exit_idle);
        BOOST_TEST(num_entering_pending == enter_pending);
        BOOST_TEST(num_entering_pending == exit_pending);

        clear_hooks();
    }
};

BOOST_AUTO_TEST_CASE(normal_lifecyle)
{
    // Initial
    mock_node nod;
    nod.check_hooks(0, 0, 0, 0);

    // First resume yields connect
    auto act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::connect);
    nod.check_hooks(0, 0, 1, 0);

    // Connect success
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check_hooks(1, 0, 0, 1);

    // Connection taken by user
    nod.mark_as_in_use();
    nod.check_hooks(0, 1, 0, 0);

    // Connection returned by user
    act = nod.resume(error_code(), collection_state::needs_collect);
    BOOST_TEST(act == next_connection_action::reset);
    nod.check_hooks(0, 0, 1, 0);

    // Reset successful
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check_hooks(1, 0, 0, 1);
}

BOOST_AUTO_TEST_CASE(connect_fail)
{
    // Get into the pending_connect state
    mock_node nod;
    auto act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::connect);
    nod.clear_hooks();

    // Fail connecting
    act = nod.resume(boost::mysql::client_errc::wrong_num_params, collection_state::none);
    BOOST_TEST(act == next_connection_action::sleep_connect_failed);
    nod.check_hooks(0, 0, 0, 0);

    // Sleep done
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::connect);
    nod.check_hooks(0, 0, 0, 0);

    // Connect success
    act = nod.resume(error_code(), collection_state::none);
    BOOST_TEST(act == next_connection_action::idle_wait);
    nod.check_hooks(1, 0, 0, 1);
}

BOOST_AUTO_TEST_SUITE_END()
