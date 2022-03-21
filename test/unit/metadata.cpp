//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/metadata.hpp>
#include "test_common.hpp"

using namespace boost::mysql::detail;
using boost::mysql::collation;
using boost::mysql::field_metadata;
using boost::mysql::field_type;

BOOST_AUTO_TEST_SUITE(test_metadata)

BOOST_AUTO_TEST_CASE(field_metadata_int_primary_key)
{
    column_definition_packet msg {
        string_lenenc("def"),
        string_lenenc("awesome"),
        string_lenenc("test_table"),
        string_lenenc("test_table"),
        string_lenenc("id"),
        string_lenenc("id"),
        collation::binary,
        11,
        protocol_field_type::long_,
        column_flags::pri_key | column_flags::auto_increment | column_flags::not_null,
        0
    };
    field_metadata meta (msg);

    BOOST_TEST(meta.database() == "awesome");
    BOOST_TEST(meta.table() == "test_table");
    BOOST_TEST(meta.original_table() == "test_table");
    BOOST_TEST(meta.field_name() == "id");
    BOOST_TEST(meta.original_field_name() == "id");
    BOOST_TEST(meta.column_length() == 11u);
    BOOST_TEST(meta.type() == field_type::int_);
    BOOST_TEST(meta.protocol_type() == protocol_field_type::long_);
    BOOST_TEST(meta.decimals() == 0u);
    BOOST_TEST(meta.is_not_null());
    BOOST_TEST(meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(field_metadata_varchar_with_alias)
{
    column_definition_packet msg {
        string_lenenc("def"),
        string_lenenc("awesome"),
        string_lenenc("child"),
        string_lenenc("child_table"),
        string_lenenc("field_alias"),
        string_lenenc("field_varchar"),
        collation::utf8_general_ci,
        765,
        protocol_field_type::var_string,
        0,
        0
    };
    field_metadata meta (msg);

    BOOST_TEST(meta.database() == "awesome");
    BOOST_TEST(meta.table() == "child");
    BOOST_TEST(meta.original_table() == "child_table");
    BOOST_TEST(meta.field_name() == "field_alias");
    BOOST_TEST(meta.original_field_name() == "field_varchar");
    BOOST_TEST(meta.column_length() == 765u);
    BOOST_TEST(meta.protocol_type() == protocol_field_type::var_string);
    BOOST_TEST(meta.type() == field_type::varchar);
    BOOST_TEST(meta.decimals() == 0u);
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(field_metadata_float_field)
{
    column_definition_packet msg {
        string_lenenc("def"),
        string_lenenc("awesome"),
        string_lenenc("test_table"),
        string_lenenc("test_table"),
        string_lenenc("field_float"),
        string_lenenc("field_float"),
        collation::binary,
        12,
        protocol_field_type::float_,
        0,
        31
    };
    field_metadata meta (msg);

    BOOST_TEST(meta.database() == "awesome");
    BOOST_TEST(meta.table() == "test_table");
    BOOST_TEST(meta.original_table() == "test_table");
    BOOST_TEST(meta.field_name() == "field_float");
    BOOST_TEST(meta.original_field_name() == "field_float");
    BOOST_TEST(meta.column_length() == 12u);
    BOOST_TEST(meta.protocol_type() == protocol_field_type::float_);
    BOOST_TEST(meta.type() == field_type::float_);
    BOOST_TEST(meta.decimals() == 31u);
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());

}

BOOST_AUTO_TEST_SUITE_END() // test_metadata
