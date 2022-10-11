//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/resultset.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/field_type.hpp>
#include "create_resultset.hpp"
#include "test_channel.hpp"
#include "test_stream.hpp"

using namespace boost::mysql::test;
using resultset_t = boost::mysql::resultset<boost::mysql::test::test_stream>;
using boost::mysql::field_type;
using boost::mysql::detail::ok_packet;
using boost::mysql::detail::resultset_encoding;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::column_definition_packet;

namespace
{

BOOST_AUTO_TEST_SUITE(test_resultset)

test_channel chan = create_channel();

BOOST_AUTO_TEST_CASE(default_ctor)
{
    resultset_t r;
    BOOST_TEST(!r.valid());
}

BOOST_AUTO_TEST_CASE(member_fns)
{
    // Construction
    resultset_t r;
    test_channel chan = create_channel();
    BOOST_TEST(!r.valid());

    // Reset
    r.reset(&chan, resultset_encoding::binary);
    BOOST_TEST(r.valid());
    BOOST_TEST(!r.complete());
    BOOST_TEST(r.meta().size() == 0);

    // Add meta
    column_definition_packet pack {};
    pack.type = protocol_field_type::var_string;
    r.add_meta(pack);
    pack.type = protocol_field_type::bit;
    r.add_meta(pack);

    BOOST_TEST(!r.complete());
    BOOST_TEST(r.meta().size() == 2);
    BOOST_TEST(r.meta()[0].type() == field_type::varchar);
    BOOST_TEST(r.meta()[1].type() == field_type::bit);

    // Complete the resultset
    r.complete(ok_packet{});
    BOOST_TEST(r.complete());

    // Reset
    r.reset(&chan, resultset_encoding::binary);
    BOOST_TEST(r.valid());
    BOOST_TEST(!r.complete());
    BOOST_TEST(r.meta().size() == 0);
}

BOOST_AUTO_TEST_CASE(move_ctor_from_invalid)
{
    resultset_t r1;
    resultset_t r2 (std::move(r1));
    BOOST_TEST(!r2.valid());
}

BOOST_AUTO_TEST_CASE(move_ctor_from_valid)
{
    auto r1 = create_resultset<resultset_t>(resultset_encoding::binary, { protocol_field_type::varchar });
    resultset_t r2 (std::move(r1));
    BOOST_TEST(r2.valid());
    BOOST_TEST(!r2.complete());
    BOOST_TEST(r2.meta().size() == 1);
}

BOOST_AUTO_TEST_CASE(move_assign_from_invalid)
{
    resultset_t r1;
    auto r2 = create_resultset<resultset_t>(resultset_encoding::text, { protocol_field_type::bit });
    r2 = std::move(r1);
    BOOST_TEST(!r2.valid());
}

BOOST_AUTO_TEST_CASE(move_assign_from_valid)
{
    auto r1 = create_resultset<resultset_t>(resultset_encoding::binary, { protocol_field_type::varchar });
    auto r2 = create_resultset<resultset_t>(resultset_encoding::text, { protocol_field_type::bit });
    r2 = std::move(r1);
    BOOST_TEST(r2.valid());
    BOOST_TEST(!r2.complete());
    BOOST_TEST(r2.meta().size() == 1);
}

BOOST_AUTO_TEST_SUITE_END()

}
