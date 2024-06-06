//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/test/unit_test.hpp>

#include <vector>

#include "test_common/create_basic.hpp"
#include "test_common/printing.hpp"
#include "test_integration/snippets/get_any_connection.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::test_tools::per_element;

namespace {

BOOST_AUTO_TEST_CASE(section_pipeline_dynamic)
{
    auto& conn = get_any_connection();

    //[pipeline_dynamic_request
    // Create a pipeline request and add three stages to it.
    // When run, this pipeline will set the connection's character set to utf8mb4
    // and prepare two statements.
    pipeline_request req;
    req.add(set_character_set_stage(utf8mb4_charset))
        .add(prepare_statement_stage("INSERT INTO audit_log (t, msg) VALUES (?, ?)"))
        .add(prepare_statement_stage(
            "INSERT INTO employee (company_id, first_name, last_name) VALUES (?, ?, ?)"
        ));
    //]

    //[pipeline_dynamic_run
    // Run the pipeline request req, and store responses into res
    // any_stage_response is a variant-like type that can store the response
    // of any stage type (including results and statements).
    std::vector<any_stage_response> res;
    conn.run_pipeline(req, res);
    //]

    //[pipeline_dynamic_results
    // The 2nd and 3rd stages were statement preparation requests,
    // so res[1] and res[2] contain statement objects
    statement stmt1 = res[1].as_statement();
    statement stmt2 = res[2].as_statement();
    //]

    BOOST_TEST(stmt1.valid());
    BOOST_TEST(stmt2.valid());
}

BOOST_AUTO_TEST_CASE(section_pipeline_static)
{
    auto& conn = get_any_connection();

    //[pipeline_static
    // Create a pipeline request containing three stages.
    // When run, this pipeline will set the connection's character set to utf8mb4
    // and prepare two statements.
    // req is a static_pipeline_request<set_character_set_stage, prepare_statement_stage,
    // prepare_statement_stage>
    auto req = make_pipeline_request(
        set_character_set_stage(utf8mb4_charset),
        prepare_statement_stage("INSERT INTO audit_log (t, msg) VALUES (?, ?)"),
        prepare_statement_stage("INSERT INTO employee (company_id, first_name, last_name) VALUES (?, ?, ?)")
    );

    // Create a response object. Instead of vectors, static pipelines use tuples.
    // Each stage type has an associated response type
    // res is a std::tuple<set_character_set_stage::response_type,
    //     prepare_statement_stage::response_type, prepare_statement_stage::response_type>
    decltype(req)::response_type res;

    // Run the pipeline
    conn.run_pipeline(req, res);

    // Access your statements
    statement stmt1 = std::get<1>(res).value();
    statement stmt2 = std::get<2>(res).value();
    //]

    BOOST_TEST(stmt1.valid());
    BOOST_TEST(stmt2.valid());
}

BOOST_AUTO_TEST_CASE(section_pipeline_errors)
{
    auto& conn = get_any_connection();

    //[pipeline_errors
    // The second step in the pipeline will fail, the other ones will succeeded
    pipeline_request req;
    req.add(set_character_set_stage(utf8mb4_charset))
        .add(prepare_statement_stage("INSERT INTO bad_table (t, msg) VALUES (?, ?)"))  // will fail
        .add(prepare_statement_stage(
            "INSERT INTO employee (company_id, first_name, last_name) VALUES (?, ?, ?)"
        ));

    std::vector<any_stage_response> res;
    error_code ec;
    diagnostics diag;

    conn.run_pipeline(req, res, ec, diag);
    BOOST_TEST(ec == common_server_errc::er_no_such_table);
    BOOST_TEST(res[0].error().code == error_code());
    BOOST_TEST(res[1].error().code == common_server_errc::er_no_such_table);
    BOOST_TEST(res[2].error().code == error_code());
    //]
}

BOOST_AUTO_TEST_CASE(section_pipeline_pitfalls)
{
    auto& conn = get_any_connection();

    {
        //[pipeline_pitfalls_bad
        // This doesn't behave correctly - DO NOT DO THIS
        // The first INSERT will fail due to a failed foreign key check (there is no such company),
        // but COMMIT will still be run, thus leaving us with an inconsistent data model
        pipeline_request req;

        req.add(execute_stage("START TRANSACTION"))
            .add(execute_stage(
                "INSERT INTO employee (first_name, last_name, company_id) VALUES ('John', 'Doe', 'bad')"
            ))
            .add(execute_stage("INSERT INTO logs VALUES ('Inserted 1 employee')"))
            .add(execute_stage("COMMIT"));
        //]

        decltype(req)::response_type res;
        error_code ec;
        diagnostics diag;
        conn.run_pipeline(req, res, ec, diag);
        BOOST_TEST(ec == error_code());  // TODO
    }

    {
        //[pipeline_pitfalls_good
        const char* sql =
            "START TRANSACTION;"
            "INSERT INTO employee (first_name, last_name, company_id) VALUES ('John', 'Doe', 'bad');"
            "INSERT INTO logs VALUES ('Inserted 1 employee');"
            "COMMIT";

        // After the first INSERT fails, nothing else will be run. This is what we want.
        // Note that you need to enable multi queries when connecting to be able to run this.
        results r;
        conn.execute(sql, r);
        //]

        // TODO: try/catch
    }
}

BOOST_AUTO_TEST_CASE(section_pipeline_reference)
{
    auto& conn = get_any_connection();
    results result;
    std::vector<any_stage_response> pipe_res;

    // Execute
    {
        pipeline_request req;
        auto stmt = conn.prepare_statement("SELECT ?, ?, ?");

        // TODO: setup params vector
        //[pipeline_reference_execute
        // Text query
        req.add(execute_stage("SELECT 1"));

        // Prepared statement, with number of parameters known at compile time
        req.add(execute_stage(stmt, {"John", "Doe", 42}));

        // Prepared statement, with number of parameters unknown at compile time
        std::vector<field_view> params{/*... */};
        req.add(execute_stage(stmt, params));
        //]

        conn.run_pipeline(req, pipe_res);
        BOOST_TEST(pipe_res.at(0).as_results().rows() == makerows(1, 1), per_element());
        BOOST_TEST(pipe_res.at(1).as_results().rows() == makerows(3, "John", "Doe", 42), per_element());
        BOOST_TEST(pipe_res.at(2).as_results().rows() == makerows(3, "John", "Doe", 42), per_element());
    }
    {
        auto stmt = conn.prepare_statement("SELECT ?, ?, ?");

        //[pipeline_reference_execute_equivalent
        // Text query
        conn.execute("SELECT 1", result);

        // Prepared statement, with number of parameters known at compile time
        conn.execute(stmt.bind("John", "Doe", 42), result);

        // Prepared statement, with number of parameters unknown at compile time
        std::vector<field_view> params{/*... */};
        conn.execute(stmt.bind(params.begin(), params.end()), result);
        //]
    }

    // Prepare statement
    {
        pipeline_request req;

        //[pipeline_reference_prepare_statement
        req.add(prepare_statement_stage("SELECT * FROM employee WHERE id = ?"));
        //]

        conn.run_pipeline(req, pipe_res);
        BOOST_TEST(pipe_res.at(0).as_statement().valid());
    }
    {
        //[pipeline_reference_prepare_statement_equivalent
        statement stmt = conn.prepare_statement("SELECT * FROM employee WHERE id = ?");
        //]

        BOOST_TEST(stmt.valid());
    }

    // Close statement
    {
        pipeline_request req;
        auto stmt = conn.prepare_statement("SELECT 1");

        //[pipeline_reference_close_statement
        req.add(close_statement_stage(stmt));
        //]

        conn.run_pipeline(req, pipe_res);
    }
    {
        auto stmt = conn.prepare_statement("SELECT 1");

        //[pipeline_reference_close_statement_equivalent
        conn.close_statement(stmt);
        //]
    }

    // Reset connection
    {
        pipeline_request req;

        //[pipeline_reference_reset_connection
        req.add(reset_connection_stage());
        //]
    }
    {
        //[pipeline_reference_reset_connection_equivalent
        conn.reset_connection();
        //]
    }

    // Set character set
    {
        pipeline_request req;
        //[pipeline_reference_set_character_set
        req.add(set_character_set_stage(utf8mb4_charset));
        //]
    }
    {
        //[pipeline_reference_set_character_set_equivalent
        conn.set_character_set(utf8mb4_charset);
        //]
    }
}

}  // namespace
