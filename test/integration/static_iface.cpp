//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/non_null.hpp>
#include <boost/mysql/static_results.hpp>

#include <boost/describe/class.hpp>
#include <boost/describe/operators.hpp>
#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <tuple>

#include "check_meta.hpp"
#include "integration_test_common.hpp"
#include "metadata_validator.hpp"
#include "printing.hpp"
#include "tcp_network_fixture.hpp"
#include "test_common.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

// Note: the dynamic interface is already covered by stored_procedures, multi_queries, prepared_statements and
// spotchecks

namespace {

BOOST_AUTO_TEST_SUITE(test_static_iface)

struct row_multifield
{
    boost::optional<float> field_nullable;
    std::int32_t field_int;
    std::string field_varchar;
};
BOOST_DESCRIBE_STRUCT(row_multifield, (), (field_nullable, field_int, field_varchar));

struct row_multifield_bad
{
    std::string field_varchar;
    float field_nullable;
    std::string field_int;
    int field_missing;
};
BOOST_DESCRIBE_STRUCT(row_multifield_bad, (), (field_varchar, field_nullable, field_int, field_missing));

struct row_2fields
{
    boost::optional<int> id;
    boost::optional<std::string> field_varchar;
};
BOOST_DESCRIBE_STRUCT(row_2fields, (), (id, field_varchar));

struct row_multifield_nonnull
{
    int id;
    non_null<float> field_nullable;
};
BOOST_DESCRIBE_STRUCT(row_multifield_nonnull, (), (id, field_nullable));

using boost::describe::operators::operator==;
using boost::describe::operators::operator<<;

using empty = std::tuple<>;

void validate_multifield_meta(metadata_collection_view meta)
{
    check_meta(
        meta,
        {
            column_type::int_,
            column_type::varchar,
            column_type::int_,
            column_type::float_,
            column_type::double_,
        }
    );
}

void validate_multified_rows(boost::span<const row_multifield> r)
{
    BOOST_TEST_REQUIRE(r.size() == 2u);
    BOOST_TEST((r[0] == row_multifield{1.1f, 11, "aaa"}));
    BOOST_TEST((r[1] == row_multifield{{}, 22, "bbb"}));
}

BOOST_AUTO_TEST_SUITE(singlefn)

BOOST_FIXTURE_TEST_CASE(describe_structs, tcp_network_fixture)
{
    connect();

    static_results<row_multifield> result;
    conn.execute("SELECT * FROM multifield_table ORDER BY id", result);

    // Verify results
    validate_multifield_meta(result.meta());
    validate_multified_rows(result.rows());
    BOOST_TEST(result.affected_rows() == 0u);
    BOOST_TEST(result.warning_count() == 0u);
    BOOST_TEST(result.last_insert_id() == 0u);  // this refers to the CALL, not to the INSERT
    BOOST_TEST(result.info() == "");
}

BOOST_FIXTURE_TEST_CASE(tuples, tcp_network_fixture)
{
    connect();

    using tuple_t = std::tuple<int, std::string, int, boost::optional<float>>;  // trailing fields discarded
    static_results<tuple_t> result;
    conn.execute("SELECT * FROM multifield_table ORDER BY id", result);

    // Verify results
    validate_multifield_meta(result.meta());
    BOOST_TEST_REQUIRE(result.rows().size() == 2u);
    BOOST_TEST((result.rows()[0] == tuple_t{1, "aaa", 11, 1.1f}));
    BOOST_TEST((result.rows()[1] == tuple_t{2, "bbb", 22, {}}));
    BOOST_TEST(result.affected_rows() == 0u);
    BOOST_TEST(result.warning_count() == 0u);
    BOOST_TEST(result.last_insert_id() == 0u);  // this refers to the CALL, not to the INSERT
    BOOST_TEST(result.info() == "");
}

BOOST_FIXTURE_TEST_CASE(multi_resultset, tcp_network_fixture)
{
    params.set_multi_queries(true);
    connect();
    start_transaction();

    static_results<row_multifield, empty, row_2fields> result;
    conn.execute(
        R"%(
            SELECT * FROM multifield_table;
            DELETE FROM updates_table;
            SELECT * FROM one_row_table
        )%",
        result
    );

    // Validate results
    validate_multifield_meta(result.meta<0>());
    validate_multified_rows(result.rows<0>());
    BOOST_TEST(result.affected_rows<0>() == 0u);
    BOOST_TEST(result.warning_count<0>() == 0u);
    BOOST_TEST(result.last_insert_id<0>() == 0u);
    BOOST_TEST(result.info<0>() == "");

    BOOST_TEST(result.meta<1>().size() == 0u);
    BOOST_TEST(result.rows<1>().size() == 0u);
    BOOST_TEST(result.affected_rows<1>() == 3u);
    BOOST_TEST(result.warning_count<1>() == 0u);
    BOOST_TEST(result.last_insert_id<1>() == 0u);
    BOOST_TEST(result.info<1>() == "");

    validate_2fields_meta(result.meta<2>(), "one_row_table");
    BOOST_TEST_REQUIRE(result.rows<2>().size() == 1u);
    BOOST_TEST_REQUIRE((result.rows<2>()[0] == row_2fields{1, std::string("f0")}));
    BOOST_TEST(result.affected_rows<2>() == 0u);
    BOOST_TEST(result.warning_count<2>() == 0u);
    BOOST_TEST(result.last_insert_id<2>() == 0u);
    BOOST_TEST(result.info<2>() == "");
}

BOOST_FIXTURE_TEST_CASE(metadata_check_failed, tcp_network_fixture)
{
    connect();

    error_code ec;
    diagnostics diag;
    static_results<row_multifield_bad> result;
    conn.execute("SELECT * FROM multifield_table ORDER BY id", result, ec, diag);

    const char* expected_msg =
        "NULL checks failed for field 'field_nullable': the database type may be NULL, but the C++ type "
        "cannot. Use std::optional<T> or boost::optional<T>\n"
        "Incompatible types for field 'field_int': C++ type 'string' is not compatible with DB type 'INT'\n"
        "Field 'field_missing' is not present in the data returned by the server";

    BOOST_TEST(ec == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_FIXTURE_TEST_CASE(metadata_check_failed_empty_resultset, tcp_network_fixture)
{
    connect();
    start_transaction();

    error_code ec;
    diagnostics diag;
    static_results<std::tuple<int>> result;
    conn.execute("DELETE FROM updates_table", result, ec, diag);

    const char* expected_msg =
        "Field in position 0 can't be mapped: there are more fields in your C++ data type than in your query";

    BOOST_TEST(ec == client_errc::metadata_check_failed);
    BOOST_TEST(diag.client_message() == expected_msg);
}

BOOST_FIXTURE_TEST_CASE(num_resultsets_mismatch, tcp_network_fixture)
{
    connect();
    start_transaction();

    error_code ec;
    diagnostics diag;
    static_results<row_2fields, empty> result;
    conn.execute("SELECT * FROM one_row_table", result, ec, diag);

    BOOST_TEST(ec == client_errc::num_resultsets_mismatch);
}

BOOST_FIXTURE_TEST_CASE(non_null_constraint_violation, tcp_network_fixture)
{
    connect();
    start_transaction();

    error_code ec;
    diagnostics diag;
    static_results<row_multifield_nonnull> result;
    conn.execute("SELECT * FROM multifield_table", result, ec, diag);

    BOOST_TEST(ec == client_errc::is_null);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
