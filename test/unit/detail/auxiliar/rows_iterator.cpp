//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <tuple>
#include <utility>
#include "test_common.hpp"

using boost::mysql::rows_view;
using boost::mysql::row_view;
using boost::mysql::rows;
using boost::mysql::field_view;
using boost::mysql::make_field_views;
using boost::mysql::test::makerow;
using boost::mysql::test::make_fv_vector;

/**
+= ptrdiff_t
    positive
    negative
    zero
-= ptrdiff_t
    positive
    negative
    zero
it + ptrdiff_t
    positive
    negative
    zero
ptrdiff_t + it
    positive
    negative
    zero
it - ptrdiff_t
    positive
    negative
    zero
it2 - it1
    positive
    negative
    zero
    self substract
operator[]
    positive
    negative
    zero
operator==, !=
    different empty objects
    different empty/nonempty objects
    different nonempty objects
    same objects, empty
    same objects, nonempty, different
    same objects, nonempty, normal/end
    same objects, same normal
    same objects, same end
operator>, <, >=, <=
    empty
    different, normal
    different, normal, end
    same, normal
    same, end
 * 
 */

BOOST_TEST_DONT_PRINT_LOG_VALUE(rows_view::const_iterator);
BOOST_TEST_DONT_PRINT_LOG_VALUE(rows::const_iterator);

namespace
{

// Provide a uniform interface for both rows and rows_view types,
// so we can use template tests to reduce duplication
struct rows_view_wrapper
{
    std::vector<field_view> fields;
    rows_view r;

    rows_view_wrapper() = default;

    template <typename... Args>
    rows_view_wrapper(std::size_t num_columns, Args&&... args) :
        fields(make_fv_vector(std::forward<Args>(args)...)),
        r(fields.data(), sizeof...(args), num_columns)
    {
    }

    rows_view::const_iterator begin() const noexcept { return r.begin(); }
    rows_view::const_iterator end() const noexcept { return r.end(); }
};

struct rows_wrapper
{
    rows r;

    rows_wrapper() = default;

    template <typename... Args>
    rows_wrapper(std::size_t num_columns, Args&&... args)
    {
        auto fields = make_field_views(std::forward<Args>(args)...);
        r = rows_view(fields.data(), fields.size(), num_columns);
    }

    rows::const_iterator begin() const noexcept { return r.begin(); }
    rows::const_iterator end() const noexcept { return r.end(); }
};

using rows_types = std::tuple<rows_view_wrapper, rows_wrapper>;

BOOST_AUTO_TEST_SUITE(test_rows_iterator)


BOOST_AUTO_TEST_SUITE(range_iteration)
BOOST_AUTO_TEST_CASE_TEMPLATE(empty, RowType, rows_types)
{
    RowType r;
    std::vector<row_view> fv (r.begin(), r.end());
    BOOST_TEST(fv.size() == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(one_row_one_column, RowType, rows_types)
{
    RowType r (1, 42);
    std::vector<row_view> fv (r.begin(), r.end());
    BOOST_TEST(fv.size() == 1);
    BOOST_TEST(fv[0] == makerow(42));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(one_row_several_columns, RowType, rows_types)
{
    RowType r (2, 80u, "abc");
    std::vector<row_view> fv (r.begin(), r.end());
    BOOST_TEST(fv.size() == 1);
    BOOST_TEST(fv[0] == makerow(80u, "abc"));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(one_column_several_rows, RowType, rows_types)
{
    RowType r (1, 42, "abc");
    std::vector<row_view> fv (r.begin(), r.end());
    BOOST_TEST(fv.size() == 2);
    BOOST_TEST(fv[0] == makerow(42));
    BOOST_TEST(fv[1] == makerow("abc"));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(several_rows_several_columns, RowType, rows_types)
{
    RowType r (2, 80u, "abc", 72u, "cde", 0u, nullptr);
    std::vector<row_view> fv (r.begin(), r.end());
    BOOST_TEST(fv.size() == 3);
    BOOST_TEST(fv[0] == makerow(80u, "abc"));
    BOOST_TEST(fv[1] == makerow(72u, "cde"));
    BOOST_TEST(fv[2] == makerow(0u, nullptr));
}
BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_CASE_TEMPLATE(prefix_increment, RowType, rows_types)
{
    RowType r (2, 80u, "abc", 72u, "cde");
    auto it = r.begin();
    BOOST_TEST(*it == makerow(80u, "abc"));

    // Increment to a dereferenceable state
    BOOST_TEST(&++it == &it);
    BOOST_TEST(*it == makerow(72u, "cde"));

    // Increment to one-past-the-end
    BOOST_TEST(&++it == &it);
    BOOST_TEST(it == r.end());
}


BOOST_AUTO_TEST_CASE_TEMPLATE(postfix_increment, RowType, rows_types)
{
    RowType r (2, 80u, "abc", 72u, "cde");
    auto it = r.begin();
    BOOST_TEST(*it == makerow(80u, "abc"));

    // Increment to a dereferenceable state
    auto itcopy = it++;
    BOOST_TEST(*itcopy == makerow(80u, "abc"));
    BOOST_TEST(*it == makerow(72u, "cde"));

    // Increment to one-past-the-end
    itcopy = it++;
    BOOST_TEST(*itcopy == makerow(72u, "cde"));
    BOOST_TEST(it == r.end());
}


// prefix decrement
//   doesn't reach end
//   reaches end
// postfix decrement
//   doesn't reach end
//   reaches end

BOOST_AUTO_TEST_SUITE_END()

}