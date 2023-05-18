//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/network_algorithms/high_level_execution.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <type_traits>

#include "assert_buffer_equals.hpp"
#include "check_meta.hpp"
#include "creation/create_execution_processor.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_meta.hpp"
#include "creation/create_row_message.hpp"
#include "creation/create_statement.hpp"
#include "printing.hpp"
#include "run_coroutine.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using detail::execution_processor;
using detail::protocol_field_type;
using detail::resultset_encoding;

namespace {

BOOST_AUTO_TEST_SUITE(test_high_level_execution)

// The serialized form of the "SELECT 1" query
constexpr std::uint8_t select_1_msg[] =
    {0x09, 0x00, 0x00, 0x00, 0x03, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20, 0x31};

// The serialized form of executing a statement with ID=1, params=("test", nullptr)
constexpr std::uint8_t execute_stmt_msg[] = {
    0x15, 0x00, 0x00, 0x00, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 0x01, 0xfe, 0x00, 0x06, 0x00, 0x04, 0x74, 0x65, 0x73, 0x74,
};

// The statement to be executed
statement create_the_statement() { return statement_builder().id(1).num_params(2).build(); }

// Verify that we clear any previous result
results create_initial_results()
{
    results res;
    add_meta(get_iface(res), {meta_builder().type(column_type::varchar).build()});
    add_row(get_iface(res), "abc");
    add_row(get_iface(res), "def");
    add_ok(get_iface(res), ok_builder().affected_rows(42).info("prev").build());
    return res;
}

execution_state create_initial_state()
{
    execution_state res;
    add_meta(get_iface(res), {meta_builder().type(column_type::time).build()});
    get_iface(res).sequence_number() = 42;
    return res;
}

//
// execute (with text query)
//
BOOST_AUTO_TEST_SUITE(execute_query)

using netfun_maker = netfun_maker_fn<void, test_channel&, const string_view&, execution_processor&>;

struct
{
    netfun_maker::signature execute;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::execute),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_execute), "async"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_initial_results();
            auto chan = create_channel();
            chan.lowest_layer()
                .add_message(create_message(1, {0x01}))  // 1 column
                .add_message(
                    create_coldef_message(2, coldef_builder().type(protocol_field_type::tiny).build())
                )
                .add_message(create_text_row_message(3, 42))
                .add_message(ok_msg_builder().seqnum(1).affected_rows(10).info("1st").build_ok());

            // Call the function
            fns.execute(chan, "SELECT 1", get_iface(result)).validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), select_1_msg);

            // Verify the results
            BOOST_TEST_REQUIRE(result.size() == 1u);
            check_meta(result.meta(), {column_type::tinyint});
            BOOST_TEST(result.affected_rows() == 10u);
            BOOST_TEST(result.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(error)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            results result;
            auto chan = create_channel();
            chan.lowest_layer().set_fail_count(fail_count(0, common_server_errc::er_aborting_connection));

            // Call the function
            fns.execute(chan, "SELECT 1", get_iface(result))
                .validate_error_exact(common_server_errc::er_aborting_connection);
        }
    }
}
BOOST_AUTO_TEST_SUITE_END()

//
// execute (statement with tuple)
//
BOOST_AUTO_TEST_SUITE(execute_statement_tuple)

using netfun_maker = netfun_maker_fn<
    void,
    test_channel&,
    const bound_statement_tuple<std::tuple<const char*, std::nullptr_t>>&,
    execution_processor&>;

struct
{
    netfun_maker::signature execute;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::execute),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_execute), "async"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_initial_results();
            auto stmt = create_the_statement();
            auto chan = create_channel();
            chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).affected_rows(50).info("1st").build_ok(
            ));

            // Call the function
            fns.execute(chan, stmt.bind("test", nullptr), get_iface(result)).validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);

            // Verify the results
            BOOST_TEST_REQUIRE(result.size() == 1u);
            BOOST_TEST(result.meta().size() == 0u);
            BOOST_TEST(result.affected_rows() == 50u);
            BOOST_TEST(result.info() == "1st");
            BOOST_TEST(get_iface(result).encoding() == resultset_encoding::binary);
        }
    }
}

BOOST_AUTO_TEST_CASE(error_wrong_num_params)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            results result;
            auto stmt = statement_builder().id(1).num_params(3).build();
            auto chan = create_channel();

            // Call the function
            fns.execute(chan, stmt.bind("test", nullptr), get_iface(result))
                .validate_error_exact(client_errc::wrong_num_params);
        }
    }
}

BOOST_AUTO_TEST_CASE(error_execute_impl)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            results result;
            auto chan = create_channel();
            chan.lowest_layer().set_fail_count(fail_count(0, common_server_errc::er_aborting_connection));

            // Call the function
            fns.execute(chan, create_the_statement().bind("test", nullptr), get_iface(result))
                .validate_error_exact(common_server_errc::er_aborting_connection);
        }
    }
}

// Verify that we correctly perform a decay-copy of the execution request,
// relevant for deferred tokens
#ifdef BOOST_ASIO_HAS_CO_AWAIT
BOOST_AUTO_TEST_CASE(deferred_lifetimes_rvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        results result;
        auto chan = create_channel();
        chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        diagnostics diag;

        // Deferred op. Execution request is a temporary
        auto aw = detail::async_execute(
            chan,
            create_the_statement().bind(std::string("test"), nullptr),
            get_iface(result),
            diag,
            boost::asio::use_awaitable
        );
        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);
        BOOST_TEST(result.info() == "1st");
    });
}

BOOST_AUTO_TEST_CASE(deferred_lifetimes_lvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        results result;
        auto chan = create_channel();
        diagnostics diag;
        chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        boost::asio::awaitable<void> aw;

        // Deferred op
        {
            auto stmt = create_the_statement();
            auto req = stmt.bind(std::string("test"), nullptr);
            aw = detail::async_execute(chan, req, get_iface(result), diag, boost::asio::use_awaitable);
        }

        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);
        BOOST_TEST(result.info() == "1st");
    });
}
#endif
BOOST_AUTO_TEST_SUITE_END()

//
// execute (statement with iterator range)
//
BOOST_AUTO_TEST_SUITE(execute_statement_iterator_range)

using netfun_maker = netfun_maker_fn<
    void,
    test_channel&,
    const bound_statement_iterator_range<const field_view*>&,
    execution_processor&>;

struct
{
    netfun_maker::signature execute;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::execute),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_execute), "async"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_initial_results();
            auto stmt = create_the_statement();
            auto chan = create_channel();
            chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).affected_rows(50).info("1st").build_ok(
            ));

            // Call the function
            const auto params = make_fv_arr("test", nullptr);
            fns.execute(chan, stmt.bind(params.data(), params.data() + params.size()), get_iface(result))
                .validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);

            // Verify the results
            BOOST_TEST_REQUIRE(result.size() == 1u);
            BOOST_TEST(result.meta().size() == 0u);
            BOOST_TEST(result.affected_rows() == 50u);
            BOOST_TEST(result.info() == "1st");
            BOOST_TEST(get_iface(result).encoding() == resultset_encoding::binary);
        }
    }
}

BOOST_AUTO_TEST_CASE(error_wrong_num_params)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            results result;
            auto stmt = statement_builder().id(1).num_params(3).build();
            auto chan = create_channel();

            // Call the function
            const auto params = make_fv_arr("test", nullptr);
            fns.execute(chan, stmt.bind(params.data(), params.data() + params.size()), get_iface(result))
                .validate_error_exact(client_errc::wrong_num_params);
        }
    }
}

// No need to test for decay copies again here, already tested
// in the tuple version.
BOOST_AUTO_TEST_SUITE_END()

//
// start_execution (text query)
//
BOOST_AUTO_TEST_SUITE(start_execution_query)

using netfun_maker = netfun_maker_fn<void, test_channel&, const string_view&, execution_processor&>;

struct
{
    netfun_maker::signature start_query;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::start_execution),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_start_execution), "async"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            std::vector<field_view> fields;
            auto st = create_initial_state();
            auto chan = create_channel();
            chan.lowest_layer()
                .add_message(create_message(1, {0x01}))  // 1 column
                .add_message(
                    create_coldef_message(2, coldef_builder().type(protocol_field_type::tiny).build())
                )
                .add_message(create_text_row_message(3, 42))
                .add_message(ok_msg_builder().seqnum(1).affected_rows(10).info("1st").build_ok());

            // Call the function
            fns.start_query(chan, "SELECT 1", get_iface(st)).validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), select_1_msg);

            // Verify the results
            BOOST_TEST(get_iface(st).encoding() == resultset_encoding::text);
            BOOST_TEST(st.should_read_rows());
            BOOST_TEST(get_iface(st).sequence_number() == 3u);
            check_meta(st.meta(), {column_type::tinyint});
        }
    }
}

BOOST_AUTO_TEST_CASE(error)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            auto chan = create_channel();
            chan.lowest_layer().set_fail_count(fail_count(0, common_server_errc::er_aborting_connection));

            // Call the function
            fns.start_query(chan, "SELECT 1", get_iface(st))
                .validate_error_exact(common_server_errc::er_aborting_connection);
        }
    }
}
BOOST_AUTO_TEST_SUITE_END()

//
// start_execution (statement, tuple)
//
BOOST_AUTO_TEST_SUITE(start_statement_execution_tuple)

using netfun_maker = netfun_maker_fn<
    void,
    test_channel&,
    const bound_statement_tuple<std::tuple<const char*, std::nullptr_t>>&,
    execution_processor&>;

struct
{
    netfun_maker::signature start_execution;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::start_execution),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_start_execution), "async"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto st = create_initial_state();
            auto stmt = create_the_statement();
            auto chan = create_channel();
            chan.lowest_layer()
                .add_message(create_message(1, {0x01}))  // 1 column
                .add_message(
                    create_coldef_message(2, coldef_builder().type(protocol_field_type::tiny).build())
                )
                .add_message(ok_msg_builder().seqnum(1).affected_rows(50).info("1st").build_ok());

            // Call the function
            fns.start_execution(chan, stmt.bind("test", nullptr), get_iface(st)).validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);

            // Verify the results
            BOOST_TEST(get_iface(st).encoding() == resultset_encoding::binary);
            BOOST_TEST(st.should_read_rows());
            BOOST_TEST(get_iface(st).sequence_number() == 3u);
            check_meta(st.meta(), {column_type::tinyint});
            BOOST_TEST(st.affected_rows() == 50u);
            BOOST_TEST(st.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_wrong_num_params)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            auto stmt = statement_builder().id(1).num_params(3).build();
            auto chan = create_channel();

            // Call the function
            fns.start_execution(chan, stmt.bind("test", nullptr), get_iface(st))
                .validate_error_exact(client_errc::wrong_num_params);
        }
    }
}

// Verify that we correctly perform a decay-copy of the parameters and the
// statement handle, relevant for deferred tokens
#ifdef BOOST_ASIO_HAS_CO_AWAIT
BOOST_AUTO_TEST_CASE(deferred_lifetimes_rvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        execution_state st;
        auto chan = create_channel();
        chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        diagnostics diag;

        // Deferred op. Execution request is a temporary
        auto aw = detail::async_start_execution(
            chan,
            create_the_statement().bind(std::string("test"), nullptr),
            get_iface(st),
            diag,
            boost::asio::use_awaitable
        );
        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);
        BOOST_TEST(st.info() == "1st");
    });
}

BOOST_AUTO_TEST_CASE(deferred_lifetimes_lvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        execution_state st;
        auto chan = create_channel();
        chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        boost::asio::awaitable<void> aw;
        diagnostics diag;

        // Deferred op
        {
            const auto stmt = create_the_statement();
            const auto req = stmt.bind(std::string("test"), nullptr);
            aw = detail::async_start_execution(chan, req, get_iface(st), diag, boost::asio::use_awaitable);
        }

        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);
        BOOST_TEST(st.info() == "1st");
    });
}
#endif
BOOST_AUTO_TEST_SUITE_END()

//
// start_execution (statement, iterator)
//
BOOST_AUTO_TEST_SUITE(start_statement_execution_it)

using netfun_maker = netfun_maker_fn<
    void,
    test_channel&,
    const bound_statement_iterator_range<field_view*>&,
    execution_processor&>;

struct
{
    netfun_maker::signature start_execution;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::start_execution),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_start_execution), "async"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto st = create_initial_state();
            auto stmt = create_the_statement();
            auto chan = create_channel();
            chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).affected_rows(50).info("1st").build_ok(
            ));

            // Call the function
            auto fields = make_fv_arr("test", nullptr);
            fns.start_execution(chan, stmt.bind(fields.data(), fields.data() + fields.size()), get_iface(st))
                .validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);

            // Verify the results
            BOOST_TEST(get_iface(st).encoding() == resultset_encoding::binary);
            BOOST_TEST(st.complete());
            BOOST_TEST(get_iface(st).sequence_number() == 2u);
            BOOST_TEST(st.meta().size() == 0u);
            BOOST_TEST(st.affected_rows() == 50u);
            BOOST_TEST(st.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_wrong_num_params)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            auto stmt = statement_builder().id(1).num_params(3).build();
            auto chan = create_channel();

            // Call the function
            auto fields = make_fv_arr("test", nullptr);
            fns.start_execution(chan, stmt.bind(fields.data(), fields.data() + fields.size()), get_iface(st))
                .validate_error_exact(client_errc::wrong_num_params);
        }
    }
}

// No need to test for decay copies again here, already tested
// in the tuple version.
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace