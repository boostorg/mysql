//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/static_execution_state.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>

#include <boost/test/unit_test.hpp>

#include "check_meta.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_meta.hpp"
#include "mock_execution_processor.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "test_connection.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::mysql::detail::execution_processor;
using boost::mysql::detail::protocol_field_type;

namespace {

BOOST_AUTO_TEST_SUITE(test_read_resultset_head)

BOOST_AUTO_TEST_SUITE(detail_)  // tests the overload that can be passed an execution_processor

using netfun_maker = netfun_maker_fn<void, test_channel&, execution_processor&>;

struct
{
    typename netfun_maker::signature read_resultset_head;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::read_resultset_head),           "sync_errc"    },
    {netfun_maker::async_errinfo(&detail::async_read_resultset_head), "async_errinfo"},
};

struct fixture
{
    test_channel chan{create_channel()};
    detail::execution_state_impl st;  // The simplest processor that stores what is passed to it

    fixture()
    {
        // The initial request writing should have advanced this to 1 (or bigger)
        st.sequence_number() = 1;
    }
};

BOOST_AUTO_TEST_CASE(success_meta)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x01});
            auto col = create_coldef_message(2, protocol_field_type::var_string);
            fix.chan.lowest_layer().add_message(concat_copy(response, col));

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // We've read the response
            BOOST_TEST(fix.st.is_reading_rows());
            BOOST_TEST(fix.st.sequence_number() == 3u);
            check_meta(fix.st.meta(), {std::make_pair(column_type::varchar, "mycol")});
        }
    }
}

BOOST_AUTO_TEST_CASE(success_several_meta_separate)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x02});
            auto col1 = create_coldef_message(2, protocol_field_type::var_string, "f1");
            auto col2 = create_coldef_message(3, protocol_field_type::tiny, "f2");
            fix.chan.lowest_layer().add_message(concat_copy(response, col1));
            fix.chan.lowest_layer().add_message(col2);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // We've read the response
            BOOST_TEST(fix.st.is_reading_rows());
            BOOST_TEST(fix.st.sequence_number() == 4u);
            check_meta(
                fix.st.meta(),
                {
                    std::make_pair(column_type::varchar, "f1"),
                    std::make_pair(column_type::tinyint, "f2"),
                }
            );
        }
    }
}

BOOST_AUTO_TEST_CASE(success_ok_packet)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = ok_msg_builder().seqnum(1).affected_rows(42).info("abc").build_ok();
            fix.chan.lowest_layer().add_message(response);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // We've read the response
            BOOST_TEST(fix.st.meta().size() == 0u);
            BOOST_TEST_REQUIRE(fix.st.is_complete());
            BOOST_TEST(fix.st.get_affected_rows() == 42u);
            BOOST_TEST(fix.st.get_info() == "abc");
        }
    }
}

// Should be a no-op
BOOST_AUTO_TEST_CASE(state_complete)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            add_ok(fix.st, ok_builder().affected_rows(42).build());

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // Nothing changed
            BOOST_TEST_REQUIRE(fix.st.is_complete());
            BOOST_TEST(fix.st.get_affected_rows() == 42u);
        }
    }
}

// Should be a no-op
BOOST_AUTO_TEST_CASE(state_reading_rows)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            add_meta(fix.st, {meta_builder().type(column_type::bit).build()});

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // Nothing changed
            BOOST_TEST_REQUIRE(fix.st.is_reading_rows());
            check_meta(fix.st.meta(), {column_type::bit});
        }
    }
}

BOOST_AUTO_TEST_CASE(error_network_error)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // This covers: error reading the initial response and
            // error reading successive metadata packets
            for (std::size_t i = 0; i <= 2; ++i)
            {
                BOOST_TEST_CONTEXT(i)
                {
                    fixture fix;
                    auto response = create_message(1, {0x02});
                    auto col1 = create_coldef_message(2, protocol_field_type::var_string, "f1");
                    auto col2 = create_coldef_message(3, protocol_field_type::tiny, "f2");
                    fix.chan.lowest_layer().add_message(response);
                    fix.chan.lowest_layer().add_message(col1);
                    fix.chan.lowest_layer().add_message(col2);
                    fix.chan.lowest_layer().set_fail_count(fail_count(i, client_errc::server_unsupported));

                    // Call the function
                    fns.read_resultset_head(fix.chan, fix.st)
                        .validate_error_exact(client_errc::server_unsupported);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(error_metadata_packets_seqnum_mismatch)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x02});
            auto col1 = create_coldef_message(2, protocol_field_type::var_string, "f1");
            auto col2 = create_coldef_message(4, protocol_field_type::tiny, "f2");
            fix.chan.lowest_layer().add_message(response);
            fix.chan.lowest_layer().add_message(col1);
            fix.chan.lowest_layer().add_message(col2);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st)
                .validate_error_exact(client_errc::sequence_number_mismatch);
        }
    }
}

// All cases where the deserialization of the execution_response
// yields an error are handled uniformly, so it's enough with this test
BOOST_AUTO_TEST_CASE(error_deserialize_execution_response)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_err_packet_message(1, common_server_errc::er_bad_db_error, "no_db");
            fix.chan.lowest_layer().add_message(response);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st)
                .validate_error_exact(common_server_errc::er_bad_db_error, "no_db");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_deserialize_metadata)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x01});
            auto col = create_message(2, {0x08, 0x03});
            fix.chan.lowest_layer().add_message(response);
            fix.chan.lowest_layer().add_message(col);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_error_exact(client_errc::incomplete_message);
        }
    }
}

BOOST_AUTO_TEST_CASE(error_on_head_ok_packet)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            mock_execution_processor proc;
            proc.sequence_number() = 1;
            proc.actions.on_head_ok_packet = [](const detail::ok_packet&, diagnostics& diag) {
                detail::diagnostics_access::assign_client(diag, "some message");
                return client_errc::metadata_check_failed;
            };

            auto response = ok_msg_builder().seqnum(1).affected_rows(42).info("abc").build_ok();
            fix.chan.lowest_layer().add_message(response);

            fns.read_resultset_head(fix.chan, proc)
                .validate_error_exact_client(client_errc::metadata_check_failed, "some message");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_on_meta)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            mock_execution_processor proc;
            proc.sequence_number() = 1;
            proc.actions.on_meta = [](metadata&&, string_view, bool, diagnostics& diag) {
                detail::diagnostics_access::assign_client(diag, "some message");
                return client_errc::metadata_check_failed;
            };

            auto response = create_message(1, {0x01});
            auto col = create_coldef_message(2, protocol_field_type::var_string);
            fix.chan.lowest_layer().add_message(concat_copy(response, col));

            fns.read_resultset_head(fix.chan, proc)
                .validate_error_exact_client(client_errc::metadata_check_failed, "some message");
        }
    }
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(connection_dynamic)  // spotchecks connection::read_resultset_head with dynamic iface

using netfun_maker = netfun_maker_mem<void, test_connection, execution_state&>;

struct
{
    typename netfun_maker::signature read_resultset_head;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::read_resultset_head),             "sync_errc"      },
    {netfun_maker::sync_exc(&test_connection::read_resultset_head),              "sync_exc"       },
    {netfun_maker::async_errinfo(&test_connection::async_read_resultset_head),   "async_errinfo"  },
    {netfun_maker::async_noerrinfo(&test_connection::async_read_resultset_head), "async_noerrinfo"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            detail::impl_access::get_impl(st).sequence_number() = 1;

            test_connection conn;
            get_channel(conn).lowest_layer().add_message(
                ok_msg_builder().seqnum(1).affected_rows(42).info("abc").build_ok()
            );

            // Call the function
            fns.read_resultset_head(conn, st).validate_no_error();

            // We've read the response
            BOOST_TEST(st.meta().size() == 0u);
            BOOST_TEST_REQUIRE(st.complete());
            BOOST_TEST(st.affected_rows() == 42u);
            BOOST_TEST(st.info() == "abc");
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
            detail::impl_access::get_impl(st).sequence_number() = 1;

            // Triggers a deserialization error, metadata message is incomplete
            test_connection conn;
            get_channel(conn)
                .lowest_layer()
                .add_message(create_message(1, {0x01}))
                .add_message(create_message(2, {0x08, 0x03}));

            // Call the function
            fns.read_resultset_head(conn, st).validate_error_exact(client_errc::incomplete_message);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(connection_static)  // spotchecks connection::read_resultset_head with static iface

using state_t = static_execution_state<std::tuple<>>;
using netfun_maker = netfun_maker_mem<void, test_connection, state_t&>;

struct
{
    typename netfun_maker::signature read_resultset_head;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::read_resultset_head),             "sync_errc"      },
    {netfun_maker::sync_exc(&test_connection::read_resultset_head),              "sync_exc"       },
    {netfun_maker::async_errinfo(&test_connection::async_read_resultset_head),   "async_errinfo"  },
    {netfun_maker::async_noerrinfo(&test_connection::async_read_resultset_head), "async_noerrinfo"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            state_t st;
            detail::impl_access::get_impl(st).get_interface().sequence_number() = 1;

            test_connection conn;
            get_channel(conn).lowest_layer().add_message(
                ok_msg_builder().seqnum(1).affected_rows(42).info("abc").build_ok()
            );

            // Call the function
            fns.read_resultset_head(conn, st).validate_no_error();

            // We've read the response
            BOOST_TEST(st.meta().size() == 0u);
            BOOST_TEST_REQUIRE(st.complete());
            BOOST_TEST(st.affected_rows() == 42u);
            BOOST_TEST(st.info() == "abc");
        }
    }
}

BOOST_AUTO_TEST_CASE(error)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            state_t st;
            detail::impl_access::get_impl(st).get_interface().sequence_number() = 1;

            // Triggers a deserialization error, metadata message is incomplete
            test_connection conn;
            get_channel(conn)
                .lowest_layer()
                .add_message(create_message(1, {0x01}))
                .add_message(create_message(2, {0x08, 0x03}));

            // Call the function
            fns.read_resultset_head(conn, st).validate_error_exact(client_errc::incomplete_message);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace