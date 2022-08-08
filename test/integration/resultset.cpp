//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/resultset.hpp>
#include <boost/mysql/row.hpp>
#include "er_connection.hpp"
#include "er_resultset.hpp"
#include "network_result.hpp"
#include "er_network_variant.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"
#include "tcp_network_fixture.hpp"
#include <boost/test/unit_test_suite.hpp>
#include <boost/utility/string_view_fwd.hpp>

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::ssl_mode;
using boost::mysql::row;
using boost::mysql::tcp_resultset;

BOOST_AUTO_TEST_SUITE(test_resultset)

// Helpers
template <class... Types>
std::vector<row> makerows(std::size_t row_size, Types&&... args)
{
    auto values = make_value_vector(std::forward<Types>(args)...);
    assert(values.size() % row_size == 0);
    std::vector<row> res;
    for (std::size_t i = 0; i < values.size(); i += row_size)
    {
        std::vector<boost::mysql::field_view> row_values (
            values.begin() + i, values.begin() + i + row_size);
        res.push_back(row(std::move(row_values), {}));
    }
    return res;
}

bool operator==(const std::vector<row>& lhs, const std::vector<row>& rhs)
{
    return boost::mysql::detail::container_equals(lhs, rhs);
}

void validate_eof(
    const er_resultset& result,
    unsigned affected_rows=0,
    unsigned warnings=0,
    unsigned last_insert=0,
    boost::string_view info=""
)
{
    BOOST_TEST_REQUIRE(result.valid());
    BOOST_TEST_REQUIRE(result.complete());
    BOOST_TEST(result.affected_rows() == affected_rows);
    BOOST_TEST(result.warning_count() == warnings);
    BOOST_TEST(result.last_insert_id() == last_insert);
    BOOST_TEST(result.info() == info);
}

// Interface to generate a resultset
class resultset_generator
{
public:
    virtual ~resultset_generator() {}
    virtual const char* name() const = 0;
    virtual er_resultset_ptr generate(er_connection&, boost::string_view) = 0;
};

class text_resultset_generator : public resultset_generator
{
public:
    const char* name() const override { return "text"; }
    er_resultset_ptr generate(er_connection& conn, boost::string_view query) override
    {
        return conn.query(query).get();
    }
};

class binary_resultset_generator : public resultset_generator
{
public:
    const char* name() const override { return "binary"; }
    er_resultset_ptr generate(er_connection& conn, boost::string_view query) override
    {
        return conn.prepare_statement(query).get()->execute_container({}).get();
    }
};

static text_resultset_generator text_obj;
static binary_resultset_generator binary_obj;

// Sample type
struct resultset_sample : network_sample
{
    resultset_generator* gen;

    resultset_sample(er_network_variant* net, resultset_generator* gen):
        network_sample(net), gen(gen)
    {
    }
};

std::ostream& operator<<(std::ostream& os, const resultset_sample& input)
{
    return os << static_cast<const network_sample&>(input) << '_' << input.gen->name();
}

struct sample_gen
{
    std::vector<resultset_sample> make_all() const
    {
        std::vector<resultset_sample> res;
        for (auto* net: all_variants())
        {
            res.emplace_back(net, &text_obj);
            res.emplace_back(net, &binary_obj);
        }
        return res;
    }

    const std::vector<resultset_sample>& operator()() const
    {
        static const auto res = make_all();
        return res;
    }
};

struct resultset_fixture : network_fixture
{
    resultset_generator* gen;

    void setup_and_connect(const resultset_sample& sample)
    {
        network_fixture::setup_and_connect(sample.net);
        gen = sample.gen;
    }

    er_resultset_ptr generate(boost::string_view query)
    {
        assert(gen);
        return gen->generate(*conn, query);
    }
};

BOOST_AUTO_TEST_SUITE(read_one)

// Verify read_one clears its paremeter correctly
static row make_initial_row()
{
    return row(make_value_vector(10), {});
}

BOOST_MYSQL_NETWORK_TEST_EX(no_results, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM empty_table");
    BOOST_TEST(result->valid());
    BOOST_TEST(!result->complete());
    BOOST_TEST(result->fields().size() == 2u);

    // Already in the end of the resultset, we receive the EOF
    row r = make_initial_row();
    bool has_row = result->read_one(r).get();
    BOOST_TEST(!has_row);
    BOOST_TEST(r == row());
    validate_eof(*result);

    // Reading again just returns null
    r = make_initial_row();
    has_row = result->read_one(r).get();
    BOOST_TEST(!has_row);
    BOOST_TEST(r == row());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST_EX(one_row, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM one_row_table");
    BOOST_TEST(result->valid());
    BOOST_TEST(!result->complete());
    BOOST_TEST(result->fields().size() == 2u);

    // Read the only row
    row r = make_initial_row();
    bool has_row = result->read_one(r).get();
    validate_2fields_meta(*result, "one_row_table");
    BOOST_TEST(has_row);
    BOOST_TEST((r == makerow(1, "f0")));
    BOOST_TEST(!result->complete());

    // Read next: end of resultset
    r = make_initial_row();
    has_row = result->read_one(r).get();
    BOOST_TEST(!has_row);
    BOOST_TEST(r == row());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST_EX(two_rows, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM two_rows_table");
    BOOST_TEST(result->valid());
    BOOST_TEST(!result->complete());
    BOOST_TEST(result->fields().size() == 2u);

    // Read first row
    row r = make_initial_row();
    bool has_row = result->read_one(r).get();
    BOOST_TEST(has_row);
    validate_2fields_meta(*result, "two_rows_table");
    BOOST_TEST((r == makerow(1, "f0")));
    BOOST_TEST(!result->complete());

    // Read next row
    r = make_initial_row();
    has_row = result->read_one(r).get();
    BOOST_TEST(has_row);
    validate_2fields_meta(*result, "two_rows_table");
    BOOST_TEST((r == makerow(2, "f1")));
    BOOST_TEST(!result->complete());

    // Read next: end of resultset
    r = make_initial_row();
    has_row = result->read_one(r).get();
    BOOST_TEST(!has_row);
    BOOST_TEST(r == row());
    validate_eof(*result);
}

// There seems to be no real case where read can fail (other than net fails)

BOOST_AUTO_TEST_SUITE_END() // read_one

BOOST_AUTO_TEST_SUITE(read_many)

BOOST_MYSQL_NETWORK_TEST_EX(no_results, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM empty_table");

    // Read many, but there are no results
    auto rows = result->read_many(10).get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);

    // Read again, should return OK and empty
    rows = result->read_many(10).get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST_EX(more_rows_than_count, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM three_rows_table");

    // Read 2, one remaining
    auto rows = result->read_many(2).get();
    BOOST_TEST(!result->complete());
    BOOST_TEST((rows == makerows(2, 1, "f0", 2, "f1")));

    // Read another two (completes the resultset)
    rows = result->read_many(2).get();
    validate_eof(*result);
    BOOST_TEST((rows == makerows(2, 3, "f2")));
}

BOOST_MYSQL_NETWORK_TEST_EX(less_rows_than_count, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM two_rows_table");

    // Read 3, resultset exhausted
    auto rows = result->read_many(3).get();
    BOOST_TEST((rows == makerows(2, 1, "f0", 2, "f1")));
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST_EX(same_rows_as_count, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM two_rows_table");

    // Read 2, 0 remaining but resultset not exhausted
    auto rows = result->read_many(2).get();
    BOOST_TEST(!result->complete());
    BOOST_TEST((rows == makerows(2, 1, "f0", 2, "f1")));

    // Read again, exhausts the resultset
    rows = result->read_many(2).get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST_EX(count_equals_one, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM one_row_table");

    // Read 1, 1 remaining
    auto rows = result->read_many(1).get();
    BOOST_TEST(!result->complete());
    BOOST_TEST((rows == makerows(2, 1, "f0")));
}

BOOST_AUTO_TEST_SUITE_END() // read_many

BOOST_AUTO_TEST_SUITE(read_all)

BOOST_MYSQL_NETWORK_TEST_EX(no_results, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM empty_table");

    // Read many, but there are no results
    auto rows = result->read_all().get();
    BOOST_TEST(rows.empty());
    BOOST_TEST(result->complete());

    // Read again, should return OK and empty
    rows = result->read_all().get();
    BOOST_TEST(rows.empty());
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST_EX(one_row, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM one_row_table");

    auto rows = result->read_all().get();
    BOOST_TEST(result->complete());
    BOOST_TEST((rows == makerows(2, 1, "f0")));
    validate_eof(*result);
}

BOOST_MYSQL_NETWORK_TEST_EX(several_rows, resultset_fixture, sample_gen())
{
    setup_and_connect(sample);
    auto result = generate("SELECT * FROM two_rows_table");

    auto rows = result->read_all().get();
    BOOST_TEST(result->complete());
    BOOST_TEST((rows == makerows(2, 1, "f0", 2, "f1")));
    validate_eof(*result);
}

BOOST_AUTO_TEST_SUITE_END() // read_all

// Move operations
BOOST_AUTO_TEST_SUITE(move_operations)

BOOST_FIXTURE_TEST_CASE(move_ctor, tcp_network_fixture)
{
    connect();

    // Get a valid resultset and perform a move construction
    tcp_resultset r = conn.query("SELECT * FROM one_row_table");
    tcp_resultset r2 (std::move(r));

    // Validate valid()
    BOOST_TEST(!r.valid());
    BOOST_TEST(r2.valid());

    // We can use the 2nd resultset
    auto rows = r2.read_all();
    BOOST_TEST((rows == makerows(2, 1, "f0")));
    BOOST_TEST(r2.complete());
}

BOOST_FIXTURE_TEST_CASE(move_assignment_to_invalid, tcp_network_fixture)
{
    connect();

    // Get a valid resultset and perform a move assignment
    tcp_resultset r = conn.query("SELECT * FROM one_row_table");
    tcp_resultset r2;
    r2 = std::move(r);

    // Validate valid()
    BOOST_TEST(!r.valid());
    BOOST_TEST(r2.valid());

    // We can use the 2nd resultset
    auto rows = r2.read_all();
    BOOST_TEST((rows == makerows(2, 1, "f0")));
    BOOST_TEST(r2.complete());
}

BOOST_FIXTURE_TEST_CASE(move_assignment_to_valid, tcp_network_fixture)
{
    connect();

    // Get a valid resultset and perform a move assignment
    tcp_resultset r2 = conn.query("SELECT * FROM empty_table");
    r2.read_all(); // clean any remaining packets
    tcp_resultset r = conn.query("SELECT * FROM one_row_table");
    r2 = std::move(r);

    // Validate valid()
    BOOST_TEST(!r.valid());
    BOOST_TEST(r2.valid());

    // We can use the 2nd resultset
    auto rows = r2.read_all();
    BOOST_TEST((rows == makerows(2, 1, "f0")));
    BOOST_TEST(r2.complete());
}

BOOST_AUTO_TEST_SUITE_END() // move_operations

BOOST_AUTO_TEST_SUITE_END() // test_resultset

