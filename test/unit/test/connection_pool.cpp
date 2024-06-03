//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/pool_params.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/connection_pool_fwd.hpp>

#include <boost/mysql/impl/internal/connection_pool/connection_node.hpp>
#include <boost/mysql/impl/internal/connection_pool/connection_pool_impl.hpp>
#include <boost/mysql/impl/internal/connection_pool/sansio_connection_node.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/test/tools/old/interface.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <memory>

#include "test_common/printing.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql;
namespace asio = boost::asio;
using detail::collection_state;

BOOST_AUTO_TEST_SUITE(test_pooled_connection)

// We use access::construct to build valid pooled_connection's instead of
// calling async_get_connection because it's faster and allows inspection of internals
struct pooled_connection_fixture
{
    asio::io_context ctx;
    std::shared_ptr<detail::pool_impl> pool{std::make_shared<detail::pool_impl>(
        pool_executor_params{ctx.get_executor(), ctx.get_executor()},
        pool_params{}
    )};

    std::unique_ptr<detail::connection_node> create_node()
    {
        return std::unique_ptr<detail::connection_node>{new detail::connection_node(
            pool->params(),
            ctx.get_executor(),
            ctx.get_executor(),
            pool->shared_state(),
            &pool->reset_pipeline_request()
        )};
    }

    pooled_connection create_valid_connection(detail::connection_node& node)
    {
        return detail::access::construct<pooled_connection>(node, pool);
    }
};

BOOST_AUTO_TEST_CASE(default_ctor)
{
    // Default-constructed connections are always invalid
    pooled_connection conn;
    BOOST_TEST(!conn.valid());
}

BOOST_FIXTURE_TEST_CASE(move_ctor_valid, pooled_connection_fixture)
{
    // Setup
    auto node = create_node();
    auto conn = create_valid_connection(*node);
    BOOST_TEST(conn.valid());

    // The moved-from connection is invalid. The node isn't marked as collectable
    pooled_connection conn2(std::move(conn));
    BOOST_TEST(conn2.valid());
    BOOST_TEST(!conn.valid());
    BOOST_TEST(node->get_collection_state() == collection_state::none);
}

BOOST_AUTO_TEST_CASE(move_ctor_invalid)
{
    // Move constructing from an invalid connection works
    pooled_connection conn;
    pooled_connection conn2(std::move(conn));
    BOOST_TEST(!conn.valid());
    BOOST_TEST(!conn2.valid());
}

BOOST_FIXTURE_TEST_CASE(move_assign_valid_valid, pooled_connection_fixture)
{
    // Setup
    auto node = create_node();
    auto node2 = create_node();
    auto conn = create_valid_connection(*node);
    auto conn2 = create_valid_connection(*node2);

    // The source is left invalid. The source node is owned by the target,
    // and the original target node is marked for collection
    conn = std::move(conn2);
    BOOST_TEST(conn.valid());
    BOOST_TEST(!conn2.valid());
    BOOST_TEST(node->get_collection_state() == collection_state::needs_collect_with_reset);
    BOOST_TEST(node2->get_collection_state() == collection_state::none);
}

BOOST_FIXTURE_TEST_CASE(move_assign_valid_invalid, pooled_connection_fixture)
{
    // Setup
    auto node = create_node();
    auto conn = create_valid_connection(*node);
    pooled_connection conn2;

    // Assigning an invalid node will mark the target for collection
    conn = std::move(conn2);
    BOOST_TEST(!conn.valid());
    BOOST_TEST(!conn2.valid());
    BOOST_TEST(node->get_collection_state() == collection_state::needs_collect_with_reset);
}

BOOST_FIXTURE_TEST_CASE(move_assign_invalid_valid, pooled_connection_fixture)
{
    // Setup
    auto node = create_node();
    pooled_connection conn;
    auto conn2 = create_valid_connection(*node);

    // Assigning a valid connection will just transfer ownership
    conn = std::move(conn2);
    BOOST_TEST(conn.valid());
    BOOST_TEST(!conn2.valid());
    BOOST_TEST(node->get_collection_state() == collection_state::none);
}

BOOST_AUTO_TEST_CASE(move_ctor_invalid_invalid)
{
    // Setup
    pooled_connection conn;
    pooled_connection conn2;

    // Moving an invalid souce to an invalid target works
    conn = std::move(conn2);
    BOOST_TEST(!conn.valid());
    BOOST_TEST(!conn2.valid());
}

BOOST_FIXTURE_TEST_CASE(return_without_reset, pooled_connection_fixture)
{
    // Setup
    auto node = create_node();
    auto node2 = create_node();
    auto conn = create_valid_connection(*node);

    // Returning without reset makes the connection invalid and sets
    // collection state
    conn.return_without_reset();
    BOOST_TEST(!conn.valid());
    BOOST_TEST(node->get_collection_state() == collection_state::needs_collect);
    ctx.poll();

    // Regression check: the reference to the pool is released
    BOOST_TEST(pool.use_count() == 1);

    // Assigning to the returned node works
    auto conn2 = create_valid_connection(*node2);
    conn = std::move(conn2);
    BOOST_TEST(conn.valid());
    BOOST_TEST(node->get_collection_state() == collection_state::needs_collect);
}

BOOST_FIXTURE_TEST_CASE(const_accessors, pooled_connection_fixture)
{
    // Const get() and operator-> work
    auto node = create_node();
    const auto conn = create_valid_connection(*node);
    BOOST_CHECK_NO_THROW(conn.get());
    BOOST_CHECK_NO_THROW(conn->uses_ssl());
}

BOOST_FIXTURE_TEST_CASE(nonconst_accessors, pooled_connection_fixture)
{
    // Non-const get() and operator-> work
    auto node = create_node();
    auto conn = create_valid_connection(*node);
    BOOST_CHECK_NO_THROW(conn.get());
    BOOST_CHECK_NO_THROW(conn->get_executor());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(test_connection_pool)

// Tests for pool constructors, assignments, and the valid() function
struct pool_fixture
{
    asio::io_context ctx;
    connection_pool pool{ctx, pool_params{}};
};

BOOST_AUTO_TEST_CASE(ctor_from_pool_executor_params)
{
    // Construct
    asio::io_context ctx1, ctx2;
    connection_pool pool(pool_executor_params{ctx1.get_executor(), ctx2.get_executor()}, pool_params{});

    // Executors are correct
    BOOST_TEST((pool.get_executor() == ctx1.get_executor()));
    BOOST_TEST((detail::access::get_impl(pool)->connection_ex() == ctx2.get_executor()));

    // The pool is valid
    BOOST_TEST(pool.valid());
}

BOOST_AUTO_TEST_CASE(ctor_from_executor)
{
    // Construct
    asio::io_context ctx;
    connection_pool pool(ctx.get_executor(), pool_params{});

    // Executors are correct
    BOOST_TEST((pool.get_executor() == ctx.get_executor()));
    BOOST_TEST((detail::access::get_impl(pool)->connection_ex() == ctx.get_executor()));

    // The pool is valid
    BOOST_TEST(pool.valid());
}

BOOST_AUTO_TEST_CASE(ctor_from_execution_context)
{
    // Construct
    asio::io_context ctx;
    connection_pool pool(ctx, pool_params{});

    // Executors are correct
    BOOST_TEST((pool.get_executor() == ctx.get_executor()));
    BOOST_TEST((detail::access::get_impl(pool)->connection_ex() == ctx.get_executor()));

    // The pool is valid
    BOOST_TEST(pool.valid());
}

BOOST_FIXTURE_TEST_CASE(move_ctor_valid, pool_fixture)
{
    // Move-constructing a pool leaves the original object invalid
    connection_pool pool2(std::move(pool));
    BOOST_TEST(!pool.valid());
    BOOST_TEST(pool2.valid());

    // The new pool works
    BOOST_CHECK_NO_THROW(pool2.cancel());
}

BOOST_FIXTURE_TEST_CASE(move_ctor_invalid, pool_fixture)
{
    // Move-constructing a pool from an invalid object yields an invalid object
    connection_pool pool2(std::move(pool));
    connection_pool pool3(std::move(pool));
    BOOST_TEST(!pool.valid());
    BOOST_TEST(!pool3.valid());
}

BOOST_FIXTURE_TEST_CASE(move_assign_valid_valid, pool_fixture)
{
    // Move-assigning a pool leaves the source invalid
    connection_pool pool2(ctx, pool_params());
    pool2 = std::move(pool);
    BOOST_TEST(!pool.valid());
    BOOST_TEST(pool2.valid());

    // The assigned pool works
    BOOST_CHECK_NO_THROW(pool2.cancel());
}

BOOST_FIXTURE_TEST_CASE(move_assign_valid_invalid, pool_fixture)
{
    // Move-assigning from an invalid pool yields an invalid pool
    connection_pool pool2(std::move(pool));
    pool2 = std::move(pool);
    BOOST_TEST(!pool.valid());
    BOOST_TEST(!pool2.valid());
}

BOOST_FIXTURE_TEST_CASE(move_assign_invalid_valid, pool_fixture)
{
    // Move-assigning to an invalid pool works
    connection_pool pool2(std::move(pool));
    pool = std::move(pool2);
    BOOST_TEST(pool.valid());
    BOOST_TEST(!pool2.valid());

    // The assigned pool works
    BOOST_CHECK_NO_THROW(pool.cancel());
}

BOOST_FIXTURE_TEST_CASE(move_assign_invalid_invalid, pool_fixture)
{
    // Move-assigning between invalid pools works
    connection_pool pool2(std::move(pool));
    connection_pool pool3(std::move(pool2));
    pool = std::move(pool2);
    BOOST_TEST(!pool.valid());
    BOOST_TEST(!pool2.valid());
}

BOOST_AUTO_TEST_SUITE_END()