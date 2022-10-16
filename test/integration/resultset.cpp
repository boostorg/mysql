//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/buffer_params.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/row_view.hpp>

#include <boost/test/unit_test.hpp>

#include "er_connection.hpp"
#include "er_network_variant.hpp"
#include "er_resultset.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::row;
using boost::mysql::row_view;

namespace {

BOOST_AUTO_TEST_SUITE(test_resultset)

void validate_eof(
    const er_resultset& result,
    unsigned affected_rows = 0,
    unsigned warnings = 0,
    unsigned last_insert = 0,
    boost::string_view info = ""
)
{
    BOOST_TEST_REQUIRE(result.base().valid());
    BOOST_TEST_REQUIRE(result.base().complete());
    BOOST_TEST(result.base().affected_rows() == affected_rows);
    BOOST_TEST(result.base().warning_count() == warnings);
    BOOST_TEST(result.base().last_insert_id() == last_insert);
    BOOST_TEST(result.base().info() == info);
}

// Interface to generate a resultset
class resultset_generator
{
public:
    virtual ~resultset_generator() {}
    virtual const char* name() const = 0;
    virtual void generate(er_connection&, er_resultset&, boost::string_view) = 0;
};

class text_resultset_generator : public resultset_generator
{
public:
    const char* name() const override { return "text"; }
    void generate(er_connection& conn, er_resultset& output, boost::string_view query) override
    {
        conn.query(query, output).validate_no_error();
    }
};

class binary_resultset_generator : public resultset_generator
{
public:
    const char* name() const override { return "binary"; }
    void generate(er_connection& conn, er_resultset& output, boost::string_view query) override
    {
        auto stmt = conn.variant().create_statement();
        conn.prepare_statement(query, *stmt).validate_no_error();
        stmt->execute_collection({}, output).validate_no_error();
    }
};

static text_resultset_generator text_obj;
static binary_resultset_generator binary_obj;

// Sample type
struct resultset_sample : network_sample
{
    resultset_generator* gen;

    resultset_sample(er_network_variant* net, resultset_generator* gen)
        : network_sample(net), gen(gen)
    {
    }
};

std::ostream& operator<<(std::ostream& os, const resultset_sample& input)
{
    return os << static_cast<const network_sample&>(input) << '_' << input.gen->name();
}

resultset_sample net_samples_subset[] = {
    resultset_sample(get_variant("tcp_sync_errc"), &text_obj),
    resultset_sample(get_variant("tcp_async_callback"), &text_obj),
    resultset_sample(get_variant("tcp_sync_exc"), &binary_obj),
    resultset_sample(get_variant("tcp_async_callback_noerrinfo"), &binary_obj),
};

std::vector<resultset_sample> net_samples_all = []() {
    std::vector<resultset_sample> res;
    for (auto* var : all_variants())
        res.emplace_back(var, &text_obj);
    return res;
}();

struct resultset_fixture : network_fixture
{
    resultset_generator* gen;

    void setup_and_connect(const resultset_sample& sample)
    {
        network_fixture::setup_and_connect(sample.net);
        gen = sample.gen;
    }

    void generate(boost::string_view query)
    {
        assert(gen);
        return gen->generate(*conn, *result, query);
    }
};

BOOST_AUTO_TEST_SUITE(read_one)

BOOST_MYSQL_NETWORK_TEST(no_results, resultset_fixture, net_samples_subset)
{
    setup_and_connect(sample);
    generate("SELECT * FROM empty_table");
    BOOST_TEST(result->base().valid());
    BOOST_TEST(!result->base().complete());
    BOOST_TEST(result->base().meta().size() == 2u);

    // Already in the end of the resultset, we receive the EOF
    row_view r = result->read_one().get();
    BOOST_TEST(r == row_view());
    validate_eof(*result);

    // Reading again does nothing
    r = result->read_one().get();
    BOOST_TEST(r == row_view());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST(one_row, resultset_fixture, net_samples_subset)
{
    setup_and_connect(sample);
    generate("SELECT * FROM one_row_table");
    BOOST_TEST(result->base().valid());
    BOOST_TEST(!result->base().complete());
    BOOST_TEST(result->base().meta().size() == 2u);

    // Read the only row
    row_view r = result->read_one().get();
    validate_2fields_meta(result->base(), "one_row_table");
    BOOST_TEST((r == makerow(1, "f0")));
    BOOST_TEST(!result->base().complete());

    // Read next: end of resultset_base
    r = result->read_one().get();
    BOOST_TEST(r == row());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST(two_rows, resultset_fixture, net_samples_all)
{
    setup_and_connect(sample);
    generate("SELECT * FROM two_rows_table");
    BOOST_TEST(result->base().valid());
    BOOST_TEST(!result->base().complete());
    BOOST_TEST(result->base().meta().size() == 2u);

    // Read first row
    row_view r = result->read_one().get();
    validate_2fields_meta(result->base(), "two_rows_table");
    BOOST_TEST((r == makerow(1, "f0")));
    BOOST_TEST(!result->base().complete());

    // Read next row
    r = result->read_one().get();
    validate_2fields_meta(result->base(), "two_rows_table");
    BOOST_TEST((r == makerow(2, "f1")));
    BOOST_TEST(!result->base().complete());

    // Read next: end of resultset_base
    r = result->read_one().get();
    BOOST_TEST(r == row());
    validate_eof(*result);
}

BOOST_AUTO_TEST_SUITE_END()  // read_one

BOOST_AUTO_TEST_SUITE(read_some)

BOOST_MYSQL_NETWORK_TEST(no_results, resultset_fixture, net_samples_subset)
{
    setup_and_connect(sample);
    generate("SELECT * FROM empty_table");

    // Read, but there are no results
    auto rows = result->read_some().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);

    // Read again, should return OK and empty
    rows = result->read_some().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST(one_row, resultset_fixture, net_samples_subset)
{
    setup_and_connect(sample);
    generate("SELECT * FROM one_row_table");

    // Read once. The resultset may or may not be complete, depending
    // on how the buffer reallocated memory
    auto rows = result->read_some().get();
    BOOST_TEST((rows == makerows(2, 1, "f0")));

    // Reading again should complete the resultset
    rows = result->read_some().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);

    // Reading again does nothing
    rows = result->read_some().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST(several_rows, resultset_fixture, net_samples_all)
{
    setup_and_connect(sample);
    generate("SELECT * FROM three_rows_table");

    // We don't know how many rows there will be in each batch,
    // but they will come in order
    std::size_t call_count = 0;
    std::vector<row> all_rows;
    while (!result->base().complete() && call_count <= 4)
    {
        ++call_count;
        for (row_view rv : result->read_some().get())
            all_rows.emplace_back(rv);
    }

    // Verify rows
    BOOST_TEST_REQUIRE(all_rows.size() == 3);
    BOOST_TEST(all_rows[0] == makerow(1, "f0"));
    BOOST_TEST(all_rows[1] == makerow(2, "f1"));
    BOOST_TEST(all_rows[2] == makerow(3, "f2"));

    // Verify eof
    validate_eof(*result);

    // Reading again does nothing
    auto rows = result->read_some().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST(several_rows_single_read, resultset_fixture, net_samples_subset)
{
    // make sure the entire result can be read at once
    params.set_buffer_config(boost::mysql::buffer_params(4096));
    setup_and_connect(sample);
    generate("SELECT * FROM three_rows_table");

    // Read
    auto rows = result->read_some().get();
    BOOST_TEST(rows == makerows(2, 1, "f0", 2, "f1", 3, "f2"));
    validate_eof(*result);

    // Reading again does nothing
    rows = result->read_some().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_AUTO_TEST_SUITE_END()  // read_many

BOOST_AUTO_TEST_SUITE(read_all)

BOOST_MYSQL_NETWORK_TEST(no_results, resultset_fixture, net_samples_subset)
{
    setup_and_connect(sample);
    generate("SELECT * FROM empty_table");

    // Read all
    auto rows = result->read_all().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);

    // Read again, should return OK and empty
    rows = result->read_all().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST(one_row, resultset_fixture, net_samples_subset)
{
    setup_and_connect(sample);
    generate("SELECT * FROM one_row_table");

    // Read all
    auto rows = result->read_all().get();
    BOOST_TEST(rows == makerows(2, 1, "f0"));
    validate_eof(*result);

    // Reading again does nothing
    rows = result->read_all().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST(several_rows, resultset_fixture, net_samples_all)
{
    setup_and_connect(sample);
    generate("SELECT * FROM two_rows_table");

    // Read all
    auto rows = result->read_all().get();
    BOOST_TEST(rows == makerows(2, 1, "f0", 2, "f1"));
    validate_eof(*result);

    // Reading again does nothing
    rows = result->read_all().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_AUTO_TEST_SUITE_END()  // read_all

BOOST_AUTO_TEST_SUITE_END()  // test_resultset

}  // namespace
