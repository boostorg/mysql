//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <boost/test/unit_test.hpp>

#include <stdexcept>

using boost::mysql::metadata;
using boost::mysql::metadata_collection_view;
using boost::mysql::detail::column_definition_packet;

namespace {

BOOST_AUTO_TEST_SUITE(test_metadata_collection_view)

metadata makemeta(const char* table_name)
{
    column_definition_packet p;
    p.table.value = table_name;
    return metadata(p, true);
}

std::vector<metadata> makemetas(const std::vector<const char*>& tables)
{
    std::vector<metadata> res;
    for (const char* t : tables)
        res.push_back(makemeta(t));
    return res;
}

// This is done via raw pointers, so no exhaustive checking
BOOST_AUTO_TEST_SUITE(range_iteration)
BOOST_AUTO_TEST_CASE(empty)
{
    metadata_collection_view v;

    BOOST_TEST(v.begin() == nullptr);
    BOOST_TEST(v.end() == nullptr);

    std::vector<metadata> vec(v.begin(), v.end());
    BOOST_TEST(vec.empty());
}

BOOST_AUTO_TEST_CASE(non_empty)
{
    auto metas = makemetas({"table1", "table2"});
    metadata_collection_view v(metas.data(), metas.size());

    BOOST_TEST(v.begin() != nullptr);
    BOOST_TEST(v.end() != nullptr);

    std::vector<metadata> vec(v.begin(), v.end());
    BOOST_TEST(vec.size() == 2u);
    BOOST_TEST(vec[0].table() == "table1");
    BOOST_TEST(vec[1].table() == "table2");
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(at)
BOOST_AUTO_TEST_CASE(empty)
{
    metadata_collection_view v;
    BOOST_CHECK_THROW(v.at(0), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(non_empty)
{
    auto metas = makemetas({"table1", "table2"});
    metadata_collection_view v(metas.data(), metas.size());
    BOOST_TEST(v.at(0).table() == "table1");
    BOOST_TEST(v.at(1).table() == "table2");
    BOOST_CHECK_THROW(v.at(2), std::out_of_range);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(operator_square_brackets)
{
    auto metas = makemetas({"table1", "table2", "table3"});
    metadata_collection_view v(metas.data(), metas.size());
    BOOST_TEST(v[0].table() == "table1");
    BOOST_TEST(v[1].table() == "table2");
    BOOST_TEST(v[2].table() == "table3");
}

BOOST_AUTO_TEST_SUITE(empty_and_size)
BOOST_AUTO_TEST_CASE(empty)
{
    metadata_collection_view v;
    BOOST_TEST(v.empty());
    BOOST_TEST(v.size() == 0u);
}

BOOST_AUTO_TEST_CASE(one_element)
{
    auto metas = makemetas({"table1"});
    metadata_collection_view v(metas.data(), metas.size());
    BOOST_TEST(!v.empty());
    BOOST_TEST(v.size() == 1u);
}

BOOST_AUTO_TEST_CASE(several_elements)
{
    auto metas = makemetas({"table1", "table2", "table3"});
    metadata_collection_view v(metas.data(), metas.size());
    BOOST_TEST(!v.empty());
    BOOST_TEST(v.size() == 3u);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
