//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob.hpp>
#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/time.hpp>

#include <boost/mysql/detail/typing/meta_check_context.hpp>
#include <boost/mysql/detail/typing/readable_field_traits.hpp>
#include <boost/mysql/detail/typing/row_traits.hpp>

#include <boost/optional/optional.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <string>

#include "creation/create_meta.hpp"
#include "custom_allocator.hpp"
#include "printing.hpp"

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#include <optional>
#endif

using namespace boost::mysql;
using namespace boost::mysql::test;
using detail::is_readable_field;
using detail::meta_check_context;

namespace {

BOOST_AUTO_TEST_SUITE(test_readable_field_traits)

//
// readable_field
//

struct other_traits : std::char_traits<char>
{
};
using string_with_alloc = std::basic_string<char, std::char_traits<char>, custom_allocator<char>>;
using string_with_traits = std::basic_string<char, other_traits>;
using blob_with_alloc = std::vector<unsigned char, custom_allocator<unsigned char>>;
using detail::meta_check_field;

struct unrelated
{
};

// scalars. char not supported directly, since may or may not be signed
static_assert(is_readable_field<unsigned char>::value, "");
static_assert(is_readable_field<signed char>::value, "");
static_assert(is_readable_field<short>::value, "");
static_assert(is_readable_field<unsigned short>::value, "");
static_assert(is_readable_field<int>::value, "");
static_assert(is_readable_field<unsigned int>::value, "");
static_assert(is_readable_field<long>::value, "");
static_assert(is_readable_field<unsigned long>::value, "");
static_assert(is_readable_field<std::int8_t>::value, "");
static_assert(is_readable_field<std::uint8_t>::value, "");
static_assert(is_readable_field<std::int16_t>::value, "");
static_assert(is_readable_field<std::uint16_t>::value, "");
static_assert(is_readable_field<std::int32_t>::value, "");
static_assert(is_readable_field<std::uint32_t>::value, "");
static_assert(is_readable_field<std::int64_t>::value, "");
static_assert(is_readable_field<std::uint64_t>::value, "");
static_assert(is_readable_field<float>::value, "");
static_assert(is_readable_field<double>::value, "");
static_assert(is_readable_field<boost::mysql::date>::value, "");
static_assert(is_readable_field<boost::mysql::datetime>::value, "");
static_assert(is_readable_field<boost::mysql::time>::value, "");
static_assert(is_readable_field<bool>::value, "");

// string types
static_assert(is_readable_field<std::string>::value, "");
static_assert(is_readable_field<string_with_alloc>::value, "");
static_assert(!is_readable_field<string_with_traits>::value, "");
static_assert(!is_readable_field<std::wstring>::value, "");
static_assert(!is_readable_field<string_view>::value, "");

// blob types
static_assert(is_readable_field<blob>::value, "");
static_assert(is_readable_field<blob_with_alloc>::value, "");
static_assert(!is_readable_field<blob_view>::value, "");

// references not accepted
static_assert(!is_readable_field<int&>::value, "");
static_assert(!is_readable_field<const int&>::value, "");
static_assert(!is_readable_field<int&&>::value, "");
static_assert(!is_readable_field<const int&>::value, "");
static_assert(!is_readable_field<std::string&>::value, "");

// optionals
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
static_assert(is_readable_field<std::optional<int>>::value, "");
static_assert(is_readable_field<std::optional<std::string>>::value, "");
#endif
static_assert(is_readable_field<boost::optional<blob>>::value, "");
static_assert(is_readable_field<boost::optional<datetime>>::value, "");
static_assert(!is_readable_field<boost::optional<void*>>::value, "");
static_assert(!is_readable_field<boost::optional<unrelated>>::value, "");
static_assert(is_readable_field<non_null<float>>::value, "");
static_assert(is_readable_field<non_null<std::string>>::value, "");
static_assert(!is_readable_field<non_null<void*>>::value, "");

// other types not accepted
static_assert(!is_readable_field<std::nullptr_t>::value, "");
static_assert(!is_readable_field<field_view>::value, "");
static_assert(!is_readable_field<field>::value, "");
static_assert(!is_readable_field<const char*>::value, "");
static_assert(!is_readable_field<void*>::value, "");
static_assert(!is_readable_field<unrelated>::value, "");
static_assert(!is_readable_field<const field_view*>::value, "");

BOOST_AUTO_TEST_SUITE(meta_check_field_)

using single_field_check_fn = void (*)(meta_check_context&);

constexpr struct cpp_type_descriptor
{
    const char* name;
    single_field_check_fn check_fn;
} cpp_type_descriptors[] = {
    {"int8_t",   &meta_check_field<std::int8_t>       },
    {"uint8_t",  &meta_check_field<std::uint8_t>      },
    {"int16_t",  &meta_check_field<std::int16_t>      },
    {"uint16_t", &meta_check_field<std::uint16_t>     },
    {"int32_t",  &meta_check_field<std::int32_t>      },
    {"uint32_t", &meta_check_field<std::uint32_t>     },
    {"int64_t",  &meta_check_field<std::int64_t>      },
    {"uint64_t", &meta_check_field<std::uint64_t>     },
    {"bool",     &meta_check_field<bool>              },
    {"float",    &meta_check_field<float>             },
    {"double",   &meta_check_field<double>            },
    {"date",     &meta_check_field<date>              },
    {"datetime", &meta_check_field<datetime>          },
    {"time",     &meta_check_field<boost::mysql::time>},
    {"string",   &meta_check_field<std::string>       },
    {"blob",     &meta_check_field<blob>              },
};
constexpr auto cpp_type_descriptors_size = sizeof(cpp_type_descriptors) / sizeof(cpp_type_descriptor);

constexpr struct db_type_descriptor
{
    const char* name;
    const char* pretty_name;
    column_type type;
    bool is_unsigned;
} db_type_descriptors[] = {
    {"TINYINT",               "tinyint",    column_type::tinyint,   false},
    {"TINYINT UNSIGNED",      "tinyintu",   column_type::tinyint,   true },
    {"SMALLINT",              "smallint",   column_type::smallint,  false},
    {"SMALLINT UNSIGNED",     "smallintu",  column_type::smallint,  true },
    {"MEDIUMINT",             "mediumint",  column_type::mediumint, false},
    {"MEDIUMINT UNSIGNED",    "mediumintu", column_type::mediumint, true },
    {"INT",                   "int",        column_type::int_,      false},
    {"INT UNSIGNED",          "intu",       column_type::int_,      true },
    {"BIGINT",                "bigint",     column_type::bigint,    false},
    {"BIGINT UNSIGNED",       "bigintu",    column_type::bigint,    true },
    {"YEAR",                  "year",       column_type::year,      true },
    {"BIT",                   "bit",        column_type::bit,       true },
    {"FLOAT",                 "float",      column_type::float_,    false},
    {"DOUBLE",                "double",     column_type::double_,   false},
    {"DATE",                  "date",       column_type::date,      false},
    {"DATETIME",              "datetime",   column_type::datetime,  false},
    {"TIMESTAMP",             "timestamp",  column_type::timestamp, false},
    {"TIME",                  "time",       column_type::time,      false},
    {"CHAR",                  "char",       column_type::char_,     false},
    {"VARCHAR",               "varchar",    column_type::varchar,   false},
    {"TEXT",                  "text",       column_type::text,      false},
    {"ENUM",                  "enum",       column_type::enum_,     false},
    {"SET",                   "set",        column_type::set,       false},
    {"JSON",                  "json",       column_type::json,      false},
    {"DECIMAL",               "decimal",    column_type::decimal,   false},
    {"BINARY",                "binary",     column_type::binary,    false},
    {"VARBINARY",             "varbinary",  column_type::varbinary, false},
    {"BLOB",                  "blob",       column_type::blob,      false},
    {"GEOMETRY",              "geometry",   column_type::geometry,  false},
    {"<unknown column type>", "unknown",    column_type::unknown,   false},
};
constexpr auto db_type_descriptors_size = sizeof(db_type_descriptors) / sizeof(db_type_descriptor);

struct compat_matrix_row
{
    std::array<bool, cpp_type_descriptors_size> compat;
};

// Looks like clang-format crashes when it sees this
// clang-format off
constexpr std::array<compat_matrix_row, db_type_descriptors_size> compat_matrix{{
    {{1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0}},  // TINYINT
    {{0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},  // TINYINT UNSIGNED
    {{0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},  // SMALLINT
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},  // SMALLINT UNSIGNED
    {{0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},  // MEDIUMINT
    {{0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},  // MEDIUMINT UNSIGNED
    {{0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},  // INT
    {{0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},  // INT UNSIGNED
    {{0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},  // BIGINT
    {{0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}},  // BIGINT UNSIGNED
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},  // YEAR
    {{0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}},  // BIT
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0}},  // FLOAT
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}},  // DOUBLE
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}},  // DATE
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},  // DATETIME
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},  // TIMESTAMP
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},  // TIME
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}},  // CHAR
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}},  // VARCHAR
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}},  // TEXT
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}},  // ENUM
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}},  // SET
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}},  // JSON
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}},  // DECIMAL
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},  // BINARY
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},  // VARBINARY
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},  // BLOB
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},  // GEOMETRY
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},  // UNKNOWN
}};
// clang-format on

BOOST_AUTO_TEST_CASE(nonnull_compatible)
{
    for (std::size_t i = 0; i < db_type_descriptors_size; ++i)
    {
        const auto& row = compat_matrix.at(i);
        for (std::size_t j = 0; j < cpp_type_descriptors_size; ++j)
        {
            if (!row.compat.at(j))
                continue;
            auto db_desc = db_type_descriptors[i];
            auto cpp_desc = cpp_type_descriptors[j];
            BOOST_TEST_CONTEXT(db_desc.pretty_name << "_" << cpp_desc.name)
            {
                auto meta = meta_builder()
                                .type(db_desc.type)
                                .unsigned_flag(db_desc.is_unsigned)
                                .nullable(false)
                                .build();
                const std::size_t pos_map[] = {0};
                meta_check_context ctx(&meta, nullptr, pos_map);

                cpp_desc.check_fn(ctx);
                diagnostics diag;
                auto err = ctx.check_errors(diag);
                BOOST_TEST(err == error_code());
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(nonnull_incompatible)
{
    for (std::size_t i = 0; i < db_type_descriptors_size; ++i)
    {
        const auto& row = compat_matrix.at(i);
        for (std::size_t j = 0; j < cpp_type_descriptors_size; ++j)
        {
            if (row.compat.at(j))
                continue;
            auto db_desc = db_type_descriptors[i];
            auto cpp_desc = cpp_type_descriptors[j];

            BOOST_TEST_CONTEXT(db_desc.pretty_name << "_" << cpp_desc.name)
            {
                auto meta = meta_builder()
                                .type(db_desc.type)
                                .unsigned_flag(db_desc.is_unsigned)
                                .nullable(false)
                                .build();
                const std::size_t pos_map[] = {0};
                meta_check_context ctx(&meta, nullptr, pos_map);

                cpp_desc.check_fn(ctx);
                diagnostics diag;
                auto err = ctx.check_errors(diag);

                BOOST_TEST(err == client_errc::metadata_check_failed);
                std::string msg = "Incompatible types for field in position 0: C++ type '" +
                                  std::string(cpp_desc.name) + "' is not compatible with DB type '" +
                                  std::string(db_desc.name) + "'";
                BOOST_TEST(diag.client_message() == msg);
            }
        }
    }
}

// meta_check:
//     optionality
// parse:
//     make parse errors non-fatal (?)
//     check for all valid types
//     check for all invalid types
BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

}  // namespace
