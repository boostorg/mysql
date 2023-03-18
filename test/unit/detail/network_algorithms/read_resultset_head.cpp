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
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>

#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "check_meta.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "test_connection.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql::test;

using boost::mysql::client_errc;
using boost::mysql::column_type;
using boost::mysql::common_server_errc;
using boost::mysql::execution_state;
using boost::mysql::field_view;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::resultset_encoding;

namespace {

using netfun_maker = netfun_maker_mem<void, test_connection, execution_state&>;

struct
{
    typename netfun_maker::signature read_resultset_head;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::read_resultset_head),           "sync" },
    {netfun_maker::async_errinfo(&test_connection::async_read_resultset_head), "async"}
};

BOOST_AUTO_TEST_SUITE(test_start_execution_generic)

struct fixture
{
    // We initiate seqnum to 1 because the initial request will have advanced it to this value
    std::vector<field_view> fields;
    execution_state st{exec_builder(false).reset(resultset_encoding::text, &fields).seqnum(1).build_state()};
    test_connection conn;

    fixture()
    {
        chan().shared_sequence_number() = 42u;
        conn.set_meta_mode(boost::mysql::metadata_mode::full);
    }

    test_channel& chan() { return get_channel(conn); }
    boost::mysql::detail::execution_state_impl& st_impl()
    {
        return boost::mysql::detail::execution_state_access::get_impl(st);
    }
};

BOOST_AUTO_TEST_CASE(success_one_meta)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x01});
            auto col = create_coldef_message(2, protocol_field_type::var_string);
            fix.chan().lowest_layer().add_message(concat_copy(response, col));

            // Call the function
            fns.read_resultset_head(fix.conn, fix.st).validate_no_error();

            // We've read the response
            BOOST_TEST(fix.st.should_read_rows());
            BOOST_TEST(fix.st_impl().sequence_number() == 3u);
            check_meta(fix.st.meta(), {std::make_pair(column_type::varchar, "mycol")});
        }
    }
}

BOOST_AUTO_TEST_CASE(success_one_meta_metadata_minimal)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x01});
            auto col = create_coldef_message(2, protocol_field_type::var_string);
            fix.chan().lowest_layer().add_message(concat_copy(response, col));
            fix.chan().set_meta_mode(boost::mysql::metadata_mode::minimal);

            // Call the function
            fns.read_resultset_head(fix.conn, fix.st).validate_no_error();

            // We've read the response
            BOOST_TEST(fix.st.should_read_rows());
            BOOST_TEST(fix.st_impl().sequence_number() == 3u);
            check_meta(fix.st.meta(), {std::make_pair(column_type::varchar, "")});
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
            fix.chan().lowest_layer().add_message(response);
            fix.chan().lowest_layer().add_message(col1);
            fix.chan().lowest_layer().add_message(col2);

            // Call the function
            fns.read_resultset_head(fix.conn, fix.st).validate_no_error();

            // We've read the response
            BOOST_TEST(fix.st.should_read_rows());
            BOOST_TEST(fix.st_impl().sequence_number() == 4u);
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
            fix.chan().lowest_layer().add_message(response);

            // Call the function
            fns.read_resultset_head(fix.conn, fix.st).validate_no_error();

            // We've read the response
            BOOST_TEST(fix.st.meta().size() == 0u);
            BOOST_TEST_REQUIRE(fix.st.complete());
            BOOST_TEST(fix.st.affected_rows() == 42u);
            BOOST_TEST(fix.st.info() == "abc");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_network_error)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // This covers: error writing the request, error reading
            // the initial response, error reading successive metadata packets
            for (std::size_t i = 0; i <= 2; ++i)
            {
                BOOST_TEST_CONTEXT(i)
                {
                    fixture fix;
                    auto response = create_message(1, {0x02});
                    auto col1 = create_coldef_message(2, protocol_field_type::var_string, "f1");
                    auto col2 = create_coldef_message(3, protocol_field_type::tiny, "f2");
                    fix.chan().lowest_layer().add_message(response);
                    fix.chan().lowest_layer().add_message(col1);
                    fix.chan().lowest_layer().add_message(col2);
                    fix.chan().lowest_layer().set_fail_count(fail_count(i, client_errc::server_unsupported));

                    // Call the function
                    fns.read_resultset_head(fix.conn, fix.st)
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
            fix.chan().lowest_layer().add_message(response);
            fix.chan().lowest_layer().add_message(col1);
            fix.chan().lowest_layer().add_message(col2);

            // Call the function
            fns.read_resultset_head(fix.conn, fix.st)
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
            fix.chan().lowest_layer().add_message(response);

            // Call the function
            fns.read_resultset_head(fix.conn, fix.st)
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
            fix.chan().lowest_layer().add_message(response);
            fix.chan().lowest_layer().add_message(col);

            // Call the function
            fns.read_resultset_head(fix.conn, fix.st).validate_error_exact(client_errc::incomplete_message);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace