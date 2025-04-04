//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/mysql_collations.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/coldef_view.hpp>
#include <boost/mysql/detail/flags.hpp>

#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <cstring>
#include <memory>

#include "test_unit/create_meta.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace collations = boost::mysql::mysql_collations;
namespace column_flags = boost::mysql::detail::column_flags;

namespace {

BOOST_AUTO_TEST_SUITE(test_metadata)

// Creates a metadata object in dynamic memory, to help sanitizers detect memory errors
std::unique_ptr<metadata> create_dynamic_meta(const detail::coldef_view& coldef, bool copy_strings)
{
    return std::unique_ptr<metadata>(new metadata(detail::access::construct<metadata>(coldef, copy_strings)));
}

// Default constructing metadata objects should be well defined
BOOST_AUTO_TEST_CASE(default_constructor)
{
    // Setup
    metadata meta;

    // Check
    BOOST_TEST(meta.database() == "");
    BOOST_TEST(meta.table() == "");
    BOOST_TEST(meta.original_table() == "");
    BOOST_TEST(meta.column_name() == "");
    BOOST_TEST(meta.original_column_name() == "");
    BOOST_TEST(meta.column_collation() == 0u);
    BOOST_TEST(meta.column_length() == 0u);
    BOOST_TEST(meta.type() == column_type::tinyint);
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

// Init ctor, copy_strings=false, there are strings to be copied in the packet
BOOST_AUTO_TEST_CASE(init_nocopy)
{
    // Setup
    auto pack = meta_builder()
                    .database("db")
                    .table("tab")
                    .org_table("org_tab")
                    .name("field")
                    .org_name("org_field")
                    .collation_id(42)
                    .type(column_type::bigint)
                    .column_length(100)
                    .decimals(200)
                    .flags(column_flags::pri_key)
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Strings were not copied. The rest of the fields were copied
    BOOST_TEST(meta.database() == "");
    BOOST_TEST(meta.table() == "");
    BOOST_TEST(meta.original_table() == "");
    BOOST_TEST(meta.column_name() == "");
    BOOST_TEST(meta.original_column_name() == "");
    BOOST_TEST(meta.column_collation() == 42u);
    BOOST_TEST(meta.column_length() == 100u);
    BOOST_TEST(meta.type() == column_type::bigint);
    BOOST_TEST(meta.decimals() == 200u);
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

// Init ctor, copy_strings=false, strings in the packet are empty
BOOST_AUTO_TEST_CASE(init_nocopy_empty_strings)
{
    // Setup
    auto pack = meta_builder().database("").table("").org_table("").name("").org_name("").build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Strings are also empty, no UB happens
    BOOST_TEST(meta.database() == "");
    BOOST_TEST(meta.table() == "");
    BOOST_TEST(meta.original_table() == "");
    BOOST_TEST(meta.column_name() == "");
    BOOST_TEST(meta.original_column_name() == "");
}

// String in dynamic storage, to help sanitizers catch memory bugs
struct dynamic_string
{
    std::unique_ptr<char[]> storage;
    std::size_t size;

    explicit dynamic_string(string_view from) : storage(new char[from.size()]), size(from.size())
    {
        std::memcpy(storage.get(), from.data(), from.size());
    }

    string_view get() const { return string_view(storage.get(), size); }
};

// Init ctor, copy_strings=true, ensure that lifetime guarantees are met
BOOST_AUTO_TEST_CASE(init_copy_lifetimes)
{
    // Construct some strings in dynamic storage, to help catch memory bugs
    dynamic_string db("db"), table("tab"), org_table("original_tab"), name("nam"), org_name("original_nam");

    // Build
    auto pack = meta_builder()
                    .database(db.get())
                    .table(table.get())
                    .org_table(org_table.get())
                    .name(name.get())
                    .org_name(org_name.get())
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Destroy the original strings
    db.storage.reset();
    table.storage.reset();
    org_table.storage.reset();
    name.storage.reset();
    org_name.storage.reset();

    // Check
    BOOST_TEST(meta.database() == "db");
    BOOST_TEST(meta.table() == "tab");
    BOOST_TEST(meta.original_table() == "original_tab");
    BOOST_TEST(meta.column_name() == "nam");
    BOOST_TEST(meta.original_column_name() == "original_nam");
}

// Init ctor, copy_strings=true, db is empty
BOOST_AUTO_TEST_CASE(init_copy_db_empty)
{
    // Setup
    auto pack = meta_builder()
                    .database("")
                    .table("Some table value")
                    .org_table("Some other original table value")
                    .name("The name of the column")
                    .org_name("The name of the original column")
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Check
    BOOST_TEST(meta.database() == "");
    BOOST_TEST(meta.table() == "Some table value");
    BOOST_TEST(meta.original_table() == "Some other original table value");
    BOOST_TEST(meta.column_name() == "The name of the column");
    BOOST_TEST(meta.original_column_name() == "The name of the original column");
}

// Same for table
BOOST_AUTO_TEST_CASE(init_copy_table_empty)
{
    // Setup
    auto pack = meta_builder()
                    .database("Database value")
                    .table(string_view())
                    .org_table("Some other original table value")
                    .name("The name of the column")
                    .org_name("The name of the original column")
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Check
    BOOST_TEST(meta.database() == "Database value");
    BOOST_TEST(meta.table() == "");
    BOOST_TEST(meta.original_table() == "Some other original table value");
    BOOST_TEST(meta.column_name() == "The name of the column");
    BOOST_TEST(meta.original_column_name() == "The name of the original column");
}

// Same for original table
BOOST_AUTO_TEST_CASE(init_copy_org_table_empty)
{
    // Setup
    auto pack = meta_builder()
                    .database("A database")
                    .table("Some table value")
                    .org_table(string_view())
                    .name("The name of the column")
                    .org_name("The name of the original column")
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Check
    BOOST_TEST(meta.database() == "A database");
    BOOST_TEST(meta.table() == "Some table value");
    BOOST_TEST(meta.original_table() == "");
    BOOST_TEST(meta.column_name() == "The name of the column");
    BOOST_TEST(meta.original_column_name() == "The name of the original column");
}

// Same for name
BOOST_AUTO_TEST_CASE(init_copy_name_empty)
{
    // Setup
    auto pack = meta_builder()
                    .database("A database")
                    .table("Some table value")
                    .org_table("Some other original table value")
                    .name(string_view())
                    .org_name("The name of the original column")
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Check
    BOOST_TEST(meta.database() == "A database");
    BOOST_TEST(meta.table() == "Some table value");
    BOOST_TEST(meta.original_table() == "Some other original table value");
    BOOST_TEST(meta.column_name() == "");
    BOOST_TEST(meta.original_column_name() == "The name of the original column");
}

// Same for org_name
BOOST_AUTO_TEST_CASE(init_copy_org_name_empty)
{
    // Setup
    auto pack = meta_builder()
                    .database("A database")
                    .table("Some table value")
                    .org_table("Some other original table value")
                    .name("The name of the column")
                    .org_name(string_view())
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Check
    BOOST_TEST(meta.database() == "A database");
    BOOST_TEST(meta.table() == "Some table value");
    BOOST_TEST(meta.original_table() == "Some other original table value");
    BOOST_TEST(meta.column_name() == "The name of the column");
    BOOST_TEST(meta.original_column_name() == "");
}

// Same, but many strings are empty
BOOST_AUTO_TEST_CASE(init_copy_many_empty)
{
    // Setup
    auto pack = meta_builder()
                    .database("A database")
                    .table(string_view())
                    .org_table(string_view())
                    .name("The name of the column")
                    .org_name(string_view())
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Check
    BOOST_TEST(meta.database() == "A database");
    BOOST_TEST(meta.table() == "");
    BOOST_TEST(meta.original_table() == "");
    BOOST_TEST(meta.column_name() == "The name of the column");
    BOOST_TEST(meta.original_column_name() == "");
}

// Same, but all strings are empty
BOOST_AUTO_TEST_CASE(init_copy_all_empty)
{
    // Setup
    auto pack = meta_builder()
                    .database(string_view())
                    .table(string_view())
                    .org_table(string_view())
                    .name(string_view())
                    .org_name(string_view())
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Check
    BOOST_TEST(meta.database() == "");
    BOOST_TEST(meta.table() == "");
    BOOST_TEST(meta.original_table() == "");
    BOOST_TEST(meta.column_name() == "");
    BOOST_TEST(meta.original_column_name() == "");
}

// copy=true does not affect how non string fields are processed
BOOST_AUTO_TEST_CASE(init_copy_nonstrings)
{
    // Setup
    auto pack = meta_builder()
                    .collation_id(42)
                    .column_length(200)
                    .type(column_type::bigint)
                    .decimals(100)
                    .flags(column_flags::pri_key)
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Check
    BOOST_TEST(meta.column_collation() == 42u);
    BOOST_TEST(meta.column_length() == 200u);
    BOOST_TEST(meta.type() == column_type::bigint);
    BOOST_TEST(meta.decimals() == 100u);
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

// Copy ctor handles strings correctly
BOOST_AUTO_TEST_CASE(copy_constructor)
{
    // Setup. Use both long and short strings to catch any SBO problems
    auto pack = meta_builder()
                    .database("db")
                    .table("Some table value")
                    .org_table("Some other original table value")
                    .name("name")
                    .org_name("The original name of the database column")
                    .column_length(200)
                    .type(column_type::blob)
                    .decimals(12)
                    .collation_id(1234)
                    .flags(column_flags::pri_key)
                    .build_coldef();
    auto meta_orig = detail::access::construct<metadata>(pack, true);

    // Copy construct
    metadata meta(meta_orig);

    // Invalidate the original object
    meta_orig = detail::access::construct<metadata>(detail::coldef_view{}, true);

    // Check
    BOOST_TEST(meta.database() == "db");
    BOOST_TEST(meta.table() == "Some table value");
    BOOST_TEST(meta.original_table() == "Some other original table value");
    BOOST_TEST(meta.column_name() == "name");
    BOOST_TEST(meta.original_column_name() == "The original name of the database column");
    BOOST_TEST(meta.column_collation() == 1234u);
    BOOST_TEST(meta.column_length() == 200u);
    BOOST_TEST(meta.type() == column_type::blob);
    BOOST_TEST(meta.decimals() == 12u);
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

// Double-check that no SBO problems happen
BOOST_AUTO_TEST_CASE(copy_constructor_sbo)
{
    // Setup. Create the original object in dynamic memory to help sanitizers
    auto pack = meta_builder()
                    .database("db")
                    .table("tab")
                    .org_table("ot")
                    .name("nam")
                    .org_name("on")
                    .build_coldef();
    auto meta_orig = create_dynamic_meta(pack, true);

    // Copy construct
    metadata meta(*meta_orig);

    // Destroy the original object
    meta_orig.reset();

    // Check
    BOOST_TEST(meta.database() == "db");
    BOOST_TEST(meta.table() == "tab");
    BOOST_TEST(meta.original_table() == "ot");
    BOOST_TEST(meta.column_name() == "nam");
    BOOST_TEST(meta.original_column_name() == "on");
}

// Copy constructor works without strings, too
BOOST_AUTO_TEST_CASE(copy_constructor_no_strings)
{
    // Setup. Use both long and short strings to catch any SBO problems
    auto pack = meta_builder().column_length(200).type(column_type::blob).build_coldef();
    auto meta_orig = detail::access::construct<metadata>(pack, false);

    // Copy construct
    metadata meta(meta_orig);

    // Check
    BOOST_TEST(meta.database() == "");
    BOOST_TEST(meta.column_length() == 200u);
    BOOST_TEST(meta.type() == column_type::blob);
}

// Move ctor handles strings correctly
BOOST_AUTO_TEST_CASE(move_constructor)
{
    // Setup. Use both long and short strings to catch any SBO problems
    auto pack = meta_builder()
                    .database("db")
                    .table("Some table value")
                    .org_table("Some other original table value")
                    .name("name")
                    .org_name("The original name of the database column")
                    .column_length(200)
                    .type(column_type::blob)
                    .decimals(12)
                    .collation_id(1234)
                    .flags(column_flags::pri_key)
                    .build_coldef();
    auto meta_orig = detail::access::construct<metadata>(pack, true);

    // Move construct
    metadata meta(std::move(meta_orig));

    // Invalidate the original object
    meta_orig = detail::access::construct<metadata>(detail::coldef_view{}, true);

    // Check
    BOOST_TEST(meta.database() == "db");
    BOOST_TEST(meta.table() == "Some table value");
    BOOST_TEST(meta.original_table() == "Some other original table value");
    BOOST_TEST(meta.column_name() == "name");
    BOOST_TEST(meta.original_column_name() == "The original name of the database column");
    BOOST_TEST(meta.column_collation() == 1234u);
    BOOST_TEST(meta.column_length() == 200u);
    BOOST_TEST(meta.type() == column_type::blob);
    BOOST_TEST(meta.decimals() == 12u);
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

// Double-check that no SBO problems happen
BOOST_AUTO_TEST_CASE(move_constructor_sbo)
{
    // Setup. Create the original object in dynamic memory to help sanitizers
    auto pack = meta_builder()
                    .database("db")
                    .table("tab")
                    .org_table("ot")
                    .name("nam")
                    .org_name("on")
                    .build_coldef();
    auto meta_orig = create_dynamic_meta(pack, true);

    // Move construct
    metadata meta(std::move(*meta_orig));

    // Destroy the original object
    meta_orig.reset();

    // Check
    BOOST_TEST(meta.database() == "db");
    BOOST_TEST(meta.table() == "tab");
    BOOST_TEST(meta.original_table() == "ot");
    BOOST_TEST(meta.column_name() == "nam");
    BOOST_TEST(meta.original_column_name() == "on");
}

// Move constructor works without strings, too
BOOST_AUTO_TEST_CASE(move_constructor_no_strings)
{
    // Setup
    auto pack = meta_builder().column_length(200).type(column_type::blob).build_coldef();
    auto meta_orig = detail::access::construct<metadata>(pack, false);

    // Copy construct
    metadata meta(std::move(meta_orig));

    // Check
    BOOST_TEST(meta.database() == "");
    BOOST_TEST(meta.column_length() == 200u);
    BOOST_TEST(meta.type() == column_type::blob);
}

// Copy assignment handles strings correctly
BOOST_AUTO_TEST_CASE(copy_assign)
{
    // Setup. Use both long and short strings to catch any SBO problems
    auto pack_orig = meta_builder()
                         .database("db")
                         .table("Some table value")
                         .org_table("Some other original table value")
                         .name("name")
                         .org_name("The original name of the database column")
                         .column_length(200)
                         .type(column_type::blob)
                         .decimals(12)
                         .collation_id(1234)
                         .flags(column_flags::pri_key)
                         .build_coldef();
    auto meta_orig = create_dynamic_meta(pack_orig, true);
    auto pack = meta_builder()
                    .database("other_db")
                    .table("another tbl")
                    .org_table("original tbl")
                    .name(string_view())
                    .org_name("Some test")
                    .column_length(10)
                    .type(column_type::varbinary)
                    .decimals(10)
                    .collation_id(42)
                    .flags(column_flags::not_null)
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Copy assign
    meta = *meta_orig;

    // Destroy the original object
    meta_orig.reset();

    // Check
    BOOST_TEST(meta.database() == "db");
    BOOST_TEST(meta.table() == "Some table value");
    BOOST_TEST(meta.original_table() == "Some other original table value");
    BOOST_TEST(meta.column_name() == "name");
    BOOST_TEST(meta.original_column_name() == "The original name of the database column");
    BOOST_TEST(meta.column_collation() == 1234u);
    BOOST_TEST(meta.column_length() == 200u);
    BOOST_TEST(meta.type() == column_type::blob);
    BOOST_TEST(meta.decimals() == 12u);
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

// Copy assignment works without strings, too
BOOST_AUTO_TEST_CASE(copy_assign_no_strings)
{
    // Setup
    auto pack_orig = meta_builder().type(column_type::blob).decimals(12).build_coldef();
    auto meta_orig = create_dynamic_meta(pack_orig, false);
    auto pack = meta_builder().type(column_type::varbinary).decimals(10).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Copy assign
    meta = *meta_orig;

    // Destroy the original object
    meta_orig.reset();

    // Check
    BOOST_TEST(meta.database() == "");
    BOOST_TEST(meta.type() == column_type::blob);
    BOOST_TEST(meta.decimals() == 12u);
}

// Self copy-assign works
BOOST_AUTO_TEST_CASE(copy_assign_self)
{
    // Setup
    auto pack = meta_builder()
                    .database("Some value")
                    .name("Some name")
                    .type(column_type::binary)
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Assign
    const auto& meta_ref = meta;  // avoid warnings
    meta = meta_ref;

    // Check
    BOOST_TEST(meta.database() == "Some value");
    BOOST_TEST(meta.column_name() == "Some name");
    BOOST_TEST(meta.type() == column_type::binary);
}

// Move assignment handles strings correctly
BOOST_AUTO_TEST_CASE(move_assign)
{
    // Setup. Use both long and short strings to catch any SBO problems
    auto pack_orig = meta_builder()
                         .database("db")
                         .table("Some table value")
                         .org_table("Some other original table value")
                         .name("name")
                         .org_name("The original name of the database column")
                         .column_length(200)
                         .type(column_type::blob)
                         .decimals(12)
                         .collation_id(1234)
                         .flags(column_flags::pri_key)
                         .build_coldef();
    auto meta_orig = create_dynamic_meta(pack_orig, true);
    auto pack = meta_builder()
                    .database("other_db")
                    .table("another tbl")
                    .org_table("original tbl")
                    .name(string_view())
                    .org_name("Some test")
                    .column_length(10)
                    .type(column_type::varbinary)
                    .decimals(10)
                    .collation_id(42)
                    .flags(column_flags::not_null)
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, true);

    // Move assign
    meta = std::move(*meta_orig);

    // Destroy the original object
    meta_orig.reset();

    // Check
    BOOST_TEST(meta.database() == "db");
    BOOST_TEST(meta.table() == "Some table value");
    BOOST_TEST(meta.original_table() == "Some other original table value");
    BOOST_TEST(meta.column_name() == "name");
    BOOST_TEST(meta.original_column_name() == "The original name of the database column");
    BOOST_TEST(meta.column_collation() == 1234u);
    BOOST_TEST(meta.column_length() == 200u);
    BOOST_TEST(meta.type() == column_type::blob);
    BOOST_TEST(meta.decimals() == 12u);
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

// Move assignment works without strings, too
BOOST_AUTO_TEST_CASE(move_assign_no_strings)
{
    // Setup. Use both long and short strings to catch any SBO problems
    auto pack_orig = meta_builder().type(column_type::blob).decimals(12).build_coldef();
    auto meta_orig = create_dynamic_meta(pack_orig, false);
    auto pack = meta_builder().type(column_type::varbinary).decimals(10).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Move assign
    meta = std::move(*meta_orig);

    // Destroy the original object
    meta_orig.reset();

    // Check
    BOOST_TEST(meta.database() == "");
    BOOST_TEST(meta.type() == column_type::blob);
    BOOST_TEST(meta.decimals() == 12u);
}

// Flags
BOOST_AUTO_TEST_CASE(flags_not_null)
{
    // Setup
    auto pack = meta_builder().flags(column_flags::not_null).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
    BOOST_TEST(meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(flags_pri_key)
{
    // Setup
    auto pack = meta_builder().flags(column_flags::pri_key).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(flags_unique_key)
{
    // Setup
    auto pack = meta_builder().flags(column_flags::unique_key).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(flags_multiple_key)
{
    // Setup
    auto pack = meta_builder().flags(column_flags::multiple_key).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(flags_unsigned)
{
    // Setup
    auto pack = meta_builder().flags(column_flags::unsigned_).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(flags_zerofill)
{
    // Setup
    auto pack = meta_builder().flags(column_flags::zerofill).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(flags_auto_increment)
{
    // Setup
    auto pack = meta_builder().flags(column_flags::auto_increment).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(flags_no_default_value)
{
    // Setup
    auto pack = meta_builder().flags(column_flags::no_default_value).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(meta.has_no_default_value());
    BOOST_TEST(!meta.is_set_to_now_on_update());
}

BOOST_AUTO_TEST_CASE(flags_on_update_now)
{
    // Setup
    auto pack = meta_builder().flags(column_flags::on_update_now).build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
    BOOST_TEST(!meta.is_not_null());
    BOOST_TEST(!meta.is_primary_key());
    BOOST_TEST(!meta.is_unique_key());
    BOOST_TEST(!meta.is_multiple_key());
    BOOST_TEST(!meta.is_unsigned());
    BOOST_TEST(!meta.is_zerofill());
    BOOST_TEST(!meta.is_auto_increment());
    BOOST_TEST(!meta.has_no_default_value());
    BOOST_TEST(meta.is_set_to_now_on_update());
}

// Several flags are set
BOOST_AUTO_TEST_CASE(flags_several)
{
    // Setup
    auto pack = meta_builder()
                    .flags(column_flags::pri_key | column_flags::auto_increment | column_flags::not_null)
                    .build_coldef();
    auto meta = detail::access::construct<metadata>(pack, false);

    // Check
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

// Some flags are not exposed
BOOST_AUTO_TEST_CASE(flags_ignored)
{
    struct
    {
        string_view name;
        std::uint16_t flags;
    } test_cases[] = {
        {"binary",    column_flags::binary                                          },
        {"enum",      column_flags::enum_                                           },
        {"timestamp", column_flags::timestamp                                       },
        {"set",       column_flags::set                                             },
        {"part_key",  column_flags::part_key                                        },
        {"num",       column_flags::num                                             },
        {"mixed",     column_flags::binary | column_flags::enum_ | column_flags::set},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            auto pack = meta_builder().flags(tc.flags).build_coldef();
            auto meta = detail::access::construct<metadata>(pack, false);

            // Check
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
    }
}

BOOST_AUTO_TEST_SUITE_END()  // test_metadata

}  // namespace