//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/mysql/resultset.hpp"
#include "boost/mysql/row.hpp"
#include "integration_test_common.hpp"
#include "test_common.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/unit_test_suite.hpp>

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::ssl_mode;
using boost::mysql::connection;
using boost::mysql::resultset;
using boost::mysql::row;
using boost::asio::ip::tcp;
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
        std::vector<boost::mysql::value> row_values (
            values.begin() + i, values.begin() + i + row_size);
        res.push_back(row(std::move(row_values), {}));
    }
    return res;
}

bool operator==(const std::vector<row>& lhs, const std::vector<row>& rhs)
{
    return boost::mysql::detail::container_equals(lhs, rhs);
}

template <class Stream>
void validate_eof(
    const resultset<Stream>& result,
    int affected_rows=0,
    int warnings=0,
    int last_insert=0,
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
template <class Stream>
class resultset_generator
{
public:
    virtual ~resultset_generator() {}
    virtual const char* name() const = 0;
    virtual resultset<Stream> generate(connection<Stream>&, boost::string_view) = 0;
};

template <class Stream>
class text_resultset_generator : public resultset_generator<Stream>
{
public:
    const char* name() const override { return "text"; }
    resultset<Stream> generate(connection<Stream>& conn, boost::string_view query) override
    {
        return conn.query(query);
    }
};

template <class Stream>
class binary_resultset_generator : public resultset_generator<Stream>
{
public:
    const char* name() const override { return "binary"; }
    resultset<Stream> generate(connection<Stream>& conn, boost::string_view query) override
    {
        return conn.prepare_statement(query).execute(boost::mysql::no_statement_params);
    }
};

// Sample type
template <class Stream>
struct resultset_sample
{
    network_functions<Stream>* net;
    ssl_mode ssl;
    resultset_generator<Stream>* gen;

    resultset_sample(network_functions<Stream>* funs, ssl_mode ssl, resultset_generator<Stream>* gen):
        net(funs),
        ssl(ssl),
        gen(gen)
    {
    }

    void set_test_attributes(boost::unit_test::test_case& test) const
    {
        test.add_label(net->name());
        test.add_label(to_string(ssl));
    }
};

template <class Stream>
std::ostream& operator<<(std::ostream& os, const resultset_sample<Stream>& input)
{
    return os << input.net->name() << '_'
              << to_string(input.ssl)
              << '_' << input.gen->name();
}

struct sample_gen
{
    template <class Stream>
    static std::vector<resultset_sample<Stream>> make_all()
    {
        static text_resultset_generator<Stream> text_obj;
        static binary_resultset_generator<Stream> binary_obj;

        resultset_generator<Stream>* all_resultset_generators [] = {
            &text_obj,
            &binary_obj
        };
        ssl_mode all_ssl_modes [] = {
            ssl_mode::disable,
            ssl_mode::require
        };

        std::vector<resultset_sample<Stream>> res;
        for (auto* net: all_network_functions<Stream>())
        {
            for (auto ssl: all_ssl_modes)
            {
                for (auto* gen: all_resultset_generators)
                {
                    res.emplace_back(net, ssl, gen);
                }
            }
        }
        return res;
    }

    template <class Stream>
    static const std::vector<resultset_sample<Stream>>& generate()
    {
        static auto res = make_all<Stream>();
        return res;
    }
};

BOOST_AUTO_TEST_SUITE(read_one)

// Verify read_one clears its paremeter correctly
static row make_initial_row()
{
    return row(make_value_vector(10), {});
}

BOOST_MYSQL_NETWORK_TEST(no_results, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM empty_table");
    BOOST_TEST(result.valid());
    BOOST_TEST(!result.complete());
    BOOST_TEST(result.fields().size() == 2);

    // Already in the end of the resultset, we receive the EOF
    row r = make_initial_row();
    auto row_result = sample.net->read_one(result, r);
    row_result.validate_no_error();
    BOOST_TEST(!row_result.value);
    BOOST_TEST(r == row());
    validate_eof(result);

    // Reading again just returns null
    r = make_initial_row();
    row_result = sample.net->read_one(result, r);
    row_result.validate_no_error();
    BOOST_TEST(!row_result.value);
    BOOST_TEST(r == row());
    validate_eof(result);
}

BOOST_MYSQL_NETWORK_TEST(one_row, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM one_row_table");
    BOOST_TEST(result.valid());
    BOOST_TEST(!result.complete());
    BOOST_TEST(result.fields().size() == 2);

    // Read the only row
    row r = make_initial_row();
    auto row_result = sample.net->read_one(result, r);
    row_result.validate_no_error();
    this->validate_2fields_meta(result, "one_row_table");
    BOOST_TEST(row_result.value);
    BOOST_TEST((r == makerow(1, "f0")));
    BOOST_TEST(!result.complete());

    // Read next: end of resultset
    r = make_initial_row();
    row_result = sample.net->read_one(result, r);
    row_result.validate_no_error();
    BOOST_TEST(!row_result.value);
    BOOST_TEST(r == row());
    validate_eof(result);
}

BOOST_MYSQL_NETWORK_TEST(two_rows, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM two_rows_table");
    BOOST_TEST(result.valid());
    BOOST_TEST(!result.complete());
    BOOST_TEST(result.fields().size() == 2);

    // Read first row
    row r = make_initial_row();
    auto row_result = sample.net->read_one(result, r);
    row_result.validate_no_error();
    BOOST_TEST(row_result.value);
    this->validate_2fields_meta(result, "two_rows_table");
    BOOST_TEST((r == makerow(1, "f0")));
    BOOST_TEST(!result.complete());

    // Read next row
    r = make_initial_row();
    row_result = sample.net->read_one(result, r);
    row_result.validate_no_error();
    BOOST_TEST(row_result.value);
    this->validate_2fields_meta(result, "two_rows_table");
    BOOST_TEST((r == makerow(2, "f1")));
    BOOST_TEST(!result.complete());

    // Read next: end of resultset
    r = make_initial_row();
    row_result = sample.net->read_one(result, r);
    row_result.validate_no_error();
    BOOST_TEST(!row_result.value);
    BOOST_TEST(r == row());
    validate_eof(result);
}

// There seems to be no real case where read can fail (other than net fails)

BOOST_AUTO_TEST_SUITE_END() // read_one

BOOST_AUTO_TEST_SUITE(read_many)

BOOST_MYSQL_NETWORK_TEST(no_results, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM empty_table");

    // Read many, but there are no results
    auto rows_result = sample.net->read_many(result, 10);
    rows_result.validate_no_error();
    BOOST_TEST(rows_result.value.empty());
    validate_eof(result);

    // Read again, should return OK and empty
    rows_result = sample.net->read_many(result, 10);
    rows_result.validate_no_error();
    BOOST_TEST(rows_result.value.empty());
    validate_eof(result);
}

BOOST_MYSQL_NETWORK_TEST(more_rows_than_count, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM three_rows_table");

    // Read 2, one remaining
    auto rows_result = sample.net->read_many(result, 2);
    rows_result.validate_no_error();
    BOOST_TEST(!result.complete());
    BOOST_TEST((rows_result.value == makerows(2, 1, "f0", 2, "f1")));

    // Read another two (completes the resultset)
    rows_result = sample.net->read_many(result, 2);
    rows_result.validate_no_error();
    validate_eof(result);
    BOOST_TEST((rows_result.value == makerows(2, 3, "f2")));
}

BOOST_MYSQL_NETWORK_TEST(less_rows_than_count, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM two_rows_table");

    // Read 3, resultset exhausted
    auto rows_result = sample.net->read_many(result, 3);
    rows_result.validate_no_error();
    BOOST_TEST((rows_result.value == makerows(2, 1, "f0", 2, "f1")));
    validate_eof(result);
}

BOOST_MYSQL_NETWORK_TEST(same_rows_as_count, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM two_rows_table");

    // Read 2, 0 remaining but resultset not exhausted
    auto rows_result = sample.net->read_many(result, 2);
    rows_result.validate_no_error();
    BOOST_TEST(!result.complete());
    BOOST_TEST((rows_result.value == makerows(2, 1, "f0", 2, "f1")));

    // Read again, exhausts the resultset
    rows_result = sample.net->read_many(result, 2);
    rows_result.validate_no_error();
    BOOST_TEST(rows_result.value.empty());
    validate_eof(result);
}

BOOST_MYSQL_NETWORK_TEST(count_equals_one, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM one_row_table");

    // Read 1, 1 remaining
    auto rows_result = sample.net->read_many(result, 1);
    rows_result.validate_no_error();
    BOOST_TEST(!result.complete());
    BOOST_TEST((rows_result.value == makerows(2, 1, "f0")));
}

BOOST_AUTO_TEST_SUITE_END() // read_many

BOOST_AUTO_TEST_SUITE(read_all)

BOOST_MYSQL_NETWORK_TEST(no_results, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM empty_table");

    // Read many, but there are no results
    auto rows_result = sample.net->read_all(result);
    rows_result.validate_no_error();
    BOOST_TEST(rows_result.value.empty());
    BOOST_TEST(result.complete());

    // Read again, should return OK and empty
    rows_result = sample.net->read_all(result);
    rows_result.validate_no_error();
    BOOST_TEST(rows_result.value.empty());
    validate_eof(result);
}

BOOST_MYSQL_NETWORK_TEST(one_row, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM one_row_table");

    auto rows_result = sample.net->read_all(result);
    rows_result.validate_no_error();
    BOOST_TEST(result.complete());
    BOOST_TEST((rows_result.value == makerows(2, 1, "f0")));
}

BOOST_MYSQL_NETWORK_TEST(several_rows, network_fixture, sample_gen)
{
    this->connect(sample.ssl);
    auto result = sample.gen->generate(this->conn,
        "SELECT * FROM two_rows_table");

    auto rows_result = sample.net->read_all(result);
    rows_result.validate_no_error();
    validate_eof(result);
    BOOST_TEST(result.complete());
    BOOST_TEST((rows_result.value == makerows(2, 1, "f0", 2, "f1")));
}

BOOST_AUTO_TEST_SUITE_END() // read_all

// Move operations
BOOST_AUTO_TEST_SUITE(move_operations)

BOOST_FIXTURE_TEST_CASE(move_ctor, network_fixture<tcp::socket>)
{
    // Get a valid resultset and perform a move construction
    this->connect(boost::mysql::ssl_mode::disable);
    tcp_resultset r = this->conn.query("SELECT * FROM one_row_table");
    tcp_resultset r2 (std::move(r));

    // Validate valid()
    BOOST_TEST(!r.valid());
    BOOST_TEST(r2.valid());

    // We can use the 2nd resultset
    auto rows = r2.read_all();
    BOOST_TEST((rows == makerows(2, 1, "f0")));
    BOOST_TEST(r2.complete());
}

BOOST_FIXTURE_TEST_CASE(move_assignment_to_invalid, network_fixture<tcp::socket>)
{
    // Get a valid resultset and perform a move assignment
    this->connect(boost::mysql::ssl_mode::disable);
    tcp_resultset r = this->conn.query("SELECT * FROM one_row_table");
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

BOOST_FIXTURE_TEST_CASE(move_assignment_to_valid, network_fixture<tcp::socket>)
{
    // Get a valid resultset and perform a move assignment
    this->connect(boost::mysql::ssl_mode::disable);
    tcp_resultset r2 = this->conn.query("SELECT * FROM empty_table");
    r2.read_all(); // clean any remaining packets
    tcp_resultset r = this->conn.query("SELECT * FROM one_row_table");
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

