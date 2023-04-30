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

constexpr struct db_type_descriptor
{
    const char* name;
    const char* pretty_name;
    column_type type;
    bool is_unsigned;
} db_type_descriptors[] = {
    {"TINYINT",            "tinyint",    column_type::tinyint,   false},
    {"TINYINT UNSIGNED",   "tinyintu",   column_type::tinyint,   true },
    {"SMALLINT",           "smallint",   column_type::smallint,  false},
    {"SMALLINT UNSIGNED",  "smallintu",  column_type::smallint,  true },
    {"MEDIUMINT",          "mediumint",  column_type::mediumint, false},
    {"MEDIUMINT UNSIGNED", "mediumintu", column_type::mediumint, true },
    {"INT",                "int",        column_type::int_,      false},
    {"INT UNSIGNED",       "intu",       column_type::int_,      true },
    {"BIGINT",             "bigint",     column_type::bigint,    false},
    {"BIGINT UNSIGNED",    "bigintu",    column_type::bigint,    true },
    {"YEAR",               "year",       column_type::year,      true },
    {"BIT",                "bit",        column_type::bit,       true },
    {"FLOAT",              "float",      column_type::float_,    false},
    {"DOUBLE",             "double",     column_type::double_,   false},
    {"DATE",               "date",       column_type::date,      false},
    {"DATETIME",           "datetime",   column_type::datetime,  false},
    {"TIMESTAMP",          "timestamp",  column_type::timestamp, false},
    {"TIME",               "time",       column_type::time,      false},
    {"CHAR",               "char",       column_type::char_,     false},
    {"VARCHAR",            "varchar",    column_type::varchar,   false},
    {"TEXT",               "text",       column_type::text,      false},
    {"ENUM",               "enum",       column_type::enum_,     false},
    {"SET",                "set",        column_type::set,       false},
    {"JSON",               "json",       column_type::json,      false},
    {"DECIMAL",            "decimal",    column_type::decimal,   false},
    {"BINARY",             "binary",     column_type::binary,    false},
    {"VARBINARY",          "varbinary",  column_type::varbinary, false},
    {"BLOB",               "blob",       column_type::blob,      false},
    {"GEOMETRY",           "geometry",   column_type::geometry,  false},
    {"UNKNOWN",            "unknown",    column_type::unknown,   false},
};

constexpr struct
{
    const char* db_type;
    std::array<bool, sizeof(cpp_type_descriptors) / sizeof(cpp_type_descriptor)> compat;
} compat_matrix[] = {
    {
     "tiny", {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
     }
};

BOOST_AUTO_TEST_CASE(nonnull_compatible)
{
    struct
    {
        const char* name;
        single_field_check_fn check_fn;
        column_type type;
        bool is_unsigned;
    } test_cases[] = {
        {"int8_t_tinyint",      &meta_check_field<std::int8_t>,        column_type::tinyint,   false},

        {"uint8_t_tinyintu",    &meta_check_field<std::uint8_t>,       column_type::tinyint,   true },

        {"int16_t_tinyint",     &meta_check_field<std::int16_t>,       column_type::tinyint,   false},
        {"int16_t_tinyintu",    &meta_check_field<std::int16_t>,       column_type::tinyint,   true },
        {"int16_t_smallint",    &meta_check_field<std::int16_t>,       column_type::smallint,  false},

        {"uint16_t_tinyintu",   &meta_check_field<std::uint16_t>,      column_type::tinyint,   true },
        {"uint16_t_smallintu",  &meta_check_field<std::uint16_t>,      column_type::smallint,  true },
        {"uint16_t_year",       &meta_check_field<std::uint16_t>,      column_type::year,      true },

        {"int32_t_tinyint",     &meta_check_field<std::int32_t>,       column_type::tinyint,   false},
        {"int32_t_tinyintu",    &meta_check_field<std::int32_t>,       column_type::tinyint,   true },
        {"int32_t_smallint",    &meta_check_field<std::int32_t>,       column_type::smallint,  false},
        {"int32_t_smallintu",   &meta_check_field<std::int32_t>,       column_type::smallint,  true },
        {"int32_t_mediumint",   &meta_check_field<std::int32_t>,       column_type::mediumint, false},
        {"int32_t_mediumintu",  &meta_check_field<std::int32_t>,       column_type::mediumint, true },
        {"int32_t_int",         &meta_check_field<std::int32_t>,       column_type::int_,      false},
        {"int32_t_year",        &meta_check_field<std::int32_t>,       column_type::year,      true },

        {"uint32_t_tinyintu",   &meta_check_field<std::uint32_t>,      column_type::tinyint,   true },
        {"uint32_t_smallintu",  &meta_check_field<std::uint32_t>,      column_type::smallint,  true },
        {"uint32_t_mediumintu", &meta_check_field<std::uint32_t>,      column_type::mediumint, true },
        {"uint32_t_intu",       &meta_check_field<std::uint32_t>,      column_type::int_,      true },
        {"uint32_t_year",       &meta_check_field<std::uint32_t>,      column_type::year,      true },

        {"int64_t_tinyint",     &meta_check_field<std::int64_t>,       column_type::tinyint,   false},
        {"int64_t_tinyintu",    &meta_check_field<std::int64_t>,       column_type::tinyint,   true },
        {"int64_t_smallint",    &meta_check_field<std::int64_t>,       column_type::smallint,  false},
        {"int64_t_smallintu",   &meta_check_field<std::int64_t>,       column_type::smallint,  true },
        {"int64_t_mediumint",   &meta_check_field<std::int64_t>,       column_type::mediumint, false},
        {"int64_t_mediumintu",  &meta_check_field<std::int64_t>,       column_type::mediumint, true },
        {"int64_t_int",         &meta_check_field<std::int64_t>,       column_type::int_,      false},
        {"int64_t_intu",        &meta_check_field<std::int64_t>,       column_type::int_,      true },
        {"int64_t_bigint",      &meta_check_field<std::int64_t>,       column_type::bigint,    false},
        {"int64_t_year",        &meta_check_field<std::int64_t>,       column_type::year,      true },

        {"uint64_t_tinyintu",   &meta_check_field<std::uint64_t>,      column_type::tinyint,   true },
        {"uint64_t_smallintu",  &meta_check_field<std::uint64_t>,      column_type::smallint,  true },
        {"uint64_t_mediumintu", &meta_check_field<std::uint64_t>,      column_type::mediumint, true },
        {"uint64_t_intu",       &meta_check_field<std::uint64_t>,      column_type::int_,      true },
        {"uint64_t_bigintu",    &meta_check_field<std::uint64_t>,      column_type::bigint,    true },
        {"uint64_t_year",       &meta_check_field<std::uint64_t>,      column_type::year,      true },
        {"uint64_t_bit",        &meta_check_field<std::uint64_t>,      column_type::bit,       true },

        {"bool_tinyint",        &meta_check_field<bool>,               column_type::tinyint,   false},

        {"float_float",         &meta_check_field<float>,              column_type::float_,    false},

        {"double_float",        &meta_check_field<double>,             column_type::float_,    false},
        {"double_double",       &meta_check_field<double>,             column_type::double_,   false},

        {"date_date",           &meta_check_field<date>,               column_type::date,      false},

        {"datetime_datetime",   &meta_check_field<datetime>,           column_type::datetime,  false},
        {"datetime_timestamp",  &meta_check_field<datetime>,           column_type::timestamp, false},

        {"time_time",           &meta_check_field<boost::mysql::time>, column_type::time,      false},

        {"string_char",         &meta_check_field<std::string>,        column_type::char_,     false},
        {"string_varchar",      &meta_check_field<std::string>,        column_type::varchar,   false},
        {"string_text",         &meta_check_field<std::string>,        column_type::text,      false},
        {"string_enum",         &meta_check_field<std::string>,        column_type::enum_,     false},
        {"string_set",          &meta_check_field<std::string>,        column_type::set,       false},
        {"string_json",         &meta_check_field<std::string>,        column_type::json,      false},
        {"string_decimal",      &meta_check_field<std::string>,        column_type::decimal,   false},

        {"blob_binary",         &meta_check_field<blob>,               column_type::binary,    false},
        {"blob_varbinary",      &meta_check_field<blob>,               column_type::varbinary, false},
        {"blob_blob",           &meta_check_field<blob>,               column_type::blob,      false},
        {"blob_geometry",       &meta_check_field<blob>,               column_type::geometry,  false},
        {"blob_unknown",        &meta_check_field<blob>,               column_type::unknown,   false},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            auto meta = meta_builder().type(tc.type).unsigned_flag(tc.is_unsigned).nullable(false).build();
            const std::size_t pos_map[] = {0};
            meta_check_context ctx(&meta, nullptr, pos_map);

            tc.check_fn(ctx);
            diagnostics diag;
            auto err = ctx.check_errors(diag);
            BOOST_TEST(err == error_code());
        }
    }
}

BOOST_AUTO_TEST_CASE(nonnull_incompatible)
{
    struct
    {
        const char* name;
        single_field_check_fn check_fn;
        column_type type;
        bool is_unsigned;
        string_view cpp_name;
        string_view db_name;
    } test_cases[] = {
  // clang-format off
        {"int8_t_tinyintu",     &meta_check_field<std::int8_t>,        column_type::tinyint,   true,  "int8_t",   "TINYINT UNSIGNED"  },
        {"int8_t_smallint",     &meta_check_field<std::int8_t>,        column_type::smallint,  false, "int8_t",   "SMALLINT"          },
        {"int8_t_smallintu",    &meta_check_field<std::int8_t>,        column_type::smallint,  true,  "int8_t",   "SMALLINT UNSIGNED" },
        {"int8_t_mediumint",    &meta_check_field<std::int8_t>,        column_type::mediumint, false, "int8_t",   "MEDIUMINT"         },
        {"int8_t_mediumintu",   &meta_check_field<std::int8_t>,        column_type::mediumint, true,  "int8_t",   "MEDIUMINT UNSIGNED"},
        {"int8_t_int",          &meta_check_field<std::int8_t>,        column_type::int_,      false, "int8_t",   "INT"               },
        {"int8_t_intu",         &meta_check_field<std::int8_t>,        column_type::int_,      true,  "int8_t",   "INT UNSIGNED"      },
        {"int8_t_bigint",       &meta_check_field<std::int8_t>,        column_type::bigint,    false, "int8_t",   "BIGINT"            },
        {"int8_t_bigintu",      &meta_check_field<std::int8_t>,        column_type::bigint,    true,  "int8_t",   "BIGINT UNSIGNED"   },
        {"int8_t_year",         &meta_check_field<std::int8_t>,        column_type::year,      true,  "int8_t",   "YEAR"              },
        {"int8_t_bit",          &meta_check_field<std::int8_t>,        column_type::bit,       true,  "int8_t",   "BIT"               },
        {"int8_t_float",        &meta_check_field<std::int8_t>,        column_type::float_,    false, "int8_t",   "FLOAT"             },
        {"int8_t_double",       &meta_check_field<std::int8_t>,        column_type::double_,   false, "int8_t",   "DOUBLE"            },
        {"int8_t_date",         &meta_check_field<std::int8_t>,        column_type::date,      false, "int8_t",   "DATE"              },
        {"int8_t_datetime",     &meta_check_field<std::int8_t>,        column_type::datetime,  false, "int8_t",   "DATETIME"          },
        {"int8_t_timestamp",    &meta_check_field<std::int8_t>,        column_type::timestamp, false, "int8_t",   "TIMESTAMP"         },
        {"int8_t_time",         &meta_check_field<std::int8_t>,        column_type::time,      false, "int8_t",   "TIME"              },
        {"int8_t_char",         &meta_check_field<std::int8_t>,        column_type::char_,     false, "int8_t",   "CHAR"              },
        {"int8_t_varchar",      &meta_check_field<std::int8_t>,        column_type::varchar,   false, "int8_t",   "VARCHAR"           },
        {"int8_t_text",         &meta_check_field<std::int8_t>,        column_type::text,      false, "int8_t",   "TEXT"              },
        {"int8_t_enum",         &meta_check_field<std::int8_t>,        column_type::enum_,     false, "int8_t",   "ENUM"              },
        {"int8_t_set",          &meta_check_field<std::int8_t>,        column_type::set,       false, "int8_t",   "SET"               },
        {"int8_t_json",         &meta_check_field<std::int8_t>,        column_type::json,      false, "int8_t",   "JSON"              },
        {"int8_t_decimal",      &meta_check_field<std::int8_t>,        column_type::decimal,   false, "int8_t",   "DECIMAL"           },
        {"int8_t_binary",       &meta_check_field<std::int8_t>,        column_type::binary,    false, "int8_t",   "BINARY"            },
        {"int8_t_varbinary",    &meta_check_field<std::int8_t>,        column_type::varbinary, false, "int8_t",   "VARBINARY"         },
        {"int8_t_blob",         &meta_check_field<std::int8_t>,        column_type::blob,      false, "int8_t",   "BLOB"              },
        {"int8_t_geometry",     &meta_check_field<std::int8_t>,        column_type::geometry,  false, "int8_t",   "GEOMETRY"          },
        {"int8_t_unknown",      &meta_check_field<std::int8_t>,        column_type::unknown,   false, "int8_t",   "<unknown column type>"           },
        {"uint8_t_tinyint",     &meta_check_field<std::uint8_t>,       column_type::tinyint,   false, "uint8_t",  "TINYINT"           },
        {"uint8_t_smallint",    &meta_check_field<std::uint8_t>,       column_type::smallint,  false, "uint8_t",  "SMALLINT"          },
        {"uint8_t_smallintu",   &meta_check_field<std::uint8_t>,       column_type::smallint,  true,  "uint8_t",  "SMALLINT UNSIGNED" },
        {"uint8_t_mediumint",   &meta_check_field<std::uint8_t>,       column_type::mediumint, false, "uint8_t",  "MEDIUMINT"         },
        {"uint8_t_mediumintu",  &meta_check_field<std::uint8_t>,       column_type::mediumint, true,  "uint8_t",  "MEDIUMINT UNSIGNED"},
        {"uint8_t_int",         &meta_check_field<std::uint8_t>,       column_type::int_,      false, "uint8_t",  "INT"               },
        {"uint8_t_intu",        &meta_check_field<std::uint8_t>,       column_type::int_,      true,  "uint8_t",  "INT UNSIGNED"      },
        {"uint8_t_bigint",      &meta_check_field<std::uint8_t>,       column_type::bigint,    false, "uint8_t",  "BIGINT"            },
        {"uint8_t_bigintu",     &meta_check_field<std::uint8_t>,       column_type::bigint,    true,  "uint8_t",  "BIGINT UNSIGNED"   },
        {"uint8_t_year",        &meta_check_field<std::uint8_t>,       column_type::year,      true,  "uint8_t",  "YEAR"              },
        {"uint8_t_bit",         &meta_check_field<std::uint8_t>,       column_type::bit,       true,  "uint8_t",  "BIT"               },
        {"uint8_t_float",       &meta_check_field<std::uint8_t>,       column_type::float_,    false, "uint8_t",  "FLOAT"             },
        {"uint8_t_double",      &meta_check_field<std::uint8_t>,       column_type::double_,   false, "uint8_t",  "DOUBLE"            },
        {"uint8_t_date",        &meta_check_field<std::uint8_t>,       column_type::date,      false, "uint8_t",  "DATE"              },
        {"uint8_t_datetime",    &meta_check_field<std::uint8_t>,       column_type::datetime,  false, "uint8_t",  "DATETIME"          },
        {"uint8_t_timestamp",   &meta_check_field<std::uint8_t>,       column_type::timestamp, false, "uint8_t",  "TIMESTAMP"         },
        {"uint8_t_time",        &meta_check_field<std::uint8_t>,       column_type::time,      false, "uint8_t",  "TIME"              },
        {"uint8_t_char",        &meta_check_field<std::uint8_t>,       column_type::char_,     false, "uint8_t",  "CHAR"              },
        {"uint8_t_varchar",     &meta_check_field<std::uint8_t>,       column_type::varchar,   false, "uint8_t",  "VARCHAR"           },
        {"uint8_t_text",        &meta_check_field<std::uint8_t>,       column_type::text,      false, "uint8_t",  "TEXT"              },
        {"uint8_t_enum",        &meta_check_field<std::uint8_t>,       column_type::enum_,     false, "uint8_t",  "ENUM"              },
        {"uint8_t_set",         &meta_check_field<std::uint8_t>,       column_type::set,       false, "uint8_t",  "SET"               },
        {"uint8_t_json",        &meta_check_field<std::uint8_t>,       column_type::json,      false, "uint8_t",  "JSON"              },
        {"uint8_t_decimal",     &meta_check_field<std::uint8_t>,       column_type::decimal,   false, "uint8_t",  "DECIMAL"           },
        {"uint8_t_binary",      &meta_check_field<std::uint8_t>,       column_type::binary,    false, "uint8_t",  "BINARY"            },
        {"uint8_t_varbinary",   &meta_check_field<std::uint8_t>,       column_type::varbinary, false, "uint8_t",  "VARBINARY"         },
        {"uint8_t_blob",        &meta_check_field<std::uint8_t>,       column_type::blob,      false, "uint8_t",  "BLOB"              },
        {"uint8_t_geometry",    &meta_check_field<std::uint8_t>,       column_type::geometry,  false, "uint8_t",  "GEOMETRY"          },
        {"uint8_t_unknown",     &meta_check_field<std::uint8_t>,       column_type::unknown,   false, "uint8_t",  "<unknown column type>"           },
        {"int16_t_smallintu",   &meta_check_field<std::int16_t>,       column_type::smallint,  true,  "int16_t",  "SMALLINT UNSIGNED" },
        {"int16_t_mediumint",   &meta_check_field<std::int16_t>,       column_type::mediumint, false, "int16_t",  "MEDIUMINT"         },
        {"int16_t_mediumintu",  &meta_check_field<std::int16_t>,       column_type::mediumint, true,  "int16_t",  "MEDIUMINT UNSIGNED"},
        {"int16_t_int",         &meta_check_field<std::int16_t>,       column_type::int_,      false, "int16_t",  "INT"               },
        {"int16_t_intu",        &meta_check_field<std::int16_t>,       column_type::int_,      true,  "int16_t",  "INT UNSIGNED"      },
        {"int16_t_bigint",      &meta_check_field<std::int16_t>,       column_type::bigint,    false, "int16_t",  "BIGINT"            },
        {"int16_t_bigintu",     &meta_check_field<std::int16_t>,       column_type::bigint,    true,  "int16_t",  "BIGINT UNSIGNED"   },
        {"int16_t_year",        &meta_check_field<std::int16_t>,       column_type::year,      true,  "int16_t",  "YEAR"              },
        {"int16_t_bit",         &meta_check_field<std::int16_t>,       column_type::bit,       true,  "int16_t",  "BIT"               },
        {"int16_t_float",       &meta_check_field<std::int16_t>,       column_type::float_,    false, "int16_t",  "FLOAT"             },
        {"int16_t_double",      &meta_check_field<std::int16_t>,       column_type::double_,   false, "int16_t",  "DOUBLE"            },
        {"int16_t_date",        &meta_check_field<std::int16_t>,       column_type::date,      false, "int16_t",  "DATE"              },
        {"int16_t_datetime",    &meta_check_field<std::int16_t>,       column_type::datetime,  false, "int16_t",  "DATETIME"          },
        {"int16_t_timestamp",   &meta_check_field<std::int16_t>,       column_type::timestamp, false, "int16_t",  "TIMESTAMP"         },
        {"int16_t_time",        &meta_check_field<std::int16_t>,       column_type::time,      false, "int16_t",  "TIME"              },
        {"int16_t_char",        &meta_check_field<std::int16_t>,       column_type::char_,     false, "int16_t",  "CHAR"              },
        {"int16_t_varchar",     &meta_check_field<std::int16_t>,       column_type::varchar,   false, "int16_t",  "VARCHAR"           },
        {"int16_t_text",        &meta_check_field<std::int16_t>,       column_type::text,      false, "int16_t",  "TEXT"              },
        {"int16_t_enum",        &meta_check_field<std::int16_t>,       column_type::enum_,     false, "int16_t",  "ENUM"              },
        {"int16_t_set",         &meta_check_field<std::int16_t>,       column_type::set,       false, "int16_t",  "SET"               },
        {"int16_t_json",        &meta_check_field<std::int16_t>,       column_type::json,      false, "int16_t",  "JSON"              },
        {"int16_t_decimal",     &meta_check_field<std::int16_t>,       column_type::decimal,   false, "int16_t",  "DECIMAL"           },
        {"int16_t_binary",      &meta_check_field<std::int16_t>,       column_type::binary,    false, "int16_t",  "BINARY"            },
        {"int16_t_varbinary",   &meta_check_field<std::int16_t>,       column_type::varbinary, false, "int16_t",  "VARBINARY"         },
        {"int16_t_blob",        &meta_check_field<std::int16_t>,       column_type::blob,      false, "int16_t",  "BLOB"              },
        {"int16_t_geometry",    &meta_check_field<std::int16_t>,       column_type::geometry,  false, "int16_t",  "GEOMETRY"          },
        {"int16_t_unknown",     &meta_check_field<std::int16_t>,       column_type::unknown,   false, "int16_t",  "<unknown column type>"           },
        {"uint16_t_tinyint",    &meta_check_field<std::uint16_t>,      column_type::tinyint,   false, "uint16_t", "TINYINT"           },
        {"uint16_t_smallint",   &meta_check_field<std::uint16_t>,      column_type::smallint,  false, "uint16_t", "SMALLINT"          },
        {"uint16_t_mediumint",  &meta_check_field<std::uint16_t>,      column_type::mediumint, false, "uint16_t", "MEDIUMINT"         },
        {"uint16_t_mediumintu", &meta_check_field<std::uint16_t>,      column_type::mediumint, true,  "uint16_t", "MEDIUMINT UNSIGNED"},
        {"uint16_t_int",        &meta_check_field<std::uint16_t>,      column_type::int_,      false, "uint16_t", "INT"               },
        {"uint16_t_intu",       &meta_check_field<std::uint16_t>,      column_type::int_,      true,  "uint16_t", "INT UNSIGNED"      },
        {"uint16_t_bigint",     &meta_check_field<std::uint16_t>,      column_type::bigint,    false, "uint16_t", "BIGINT"            },
        {"uint16_t_bigintu",    &meta_check_field<std::uint16_t>,      column_type::bigint,    true,  "uint16_t", "BIGINT UNSIGNED"   },
        {"uint16_t_bit",        &meta_check_field<std::uint16_t>,      column_type::bit,       true,  "uint16_t", "BIT"               },
        {"uint16_t_float",      &meta_check_field<std::uint16_t>,      column_type::float_,    false, "uint16_t", "FLOAT"             },
        {"uint16_t_double",     &meta_check_field<std::uint16_t>,      column_type::double_,   false, "uint16_t", "DOUBLE"            },
        {"uint16_t_date",       &meta_check_field<std::uint16_t>,      column_type::date,      false, "uint16_t", "DATE"              },
        {"uint16_t_datetime",   &meta_check_field<std::uint16_t>,      column_type::datetime,  false, "uint16_t", "DATETIME"          },
        {"uint16_t_timestamp",  &meta_check_field<std::uint16_t>,      column_type::timestamp, false, "uint16_t", "TIMESTAMP"         },
        {"uint16_t_time",       &meta_check_field<std::uint16_t>,      column_type::time,      false, "uint16_t", "TIME"              },
        {"uint16_t_char",       &meta_check_field<std::uint16_t>,      column_type::char_,     false, "uint16_t", "CHAR"              },
        {"uint16_t_varchar",    &meta_check_field<std::uint16_t>,      column_type::varchar,   false, "uint16_t", "VARCHAR"           },
        {"uint16_t_text",       &meta_check_field<std::uint16_t>,      column_type::text,      false, "uint16_t", "TEXT"              },
        {"uint16_t_enum",       &meta_check_field<std::uint16_t>,      column_type::enum_,     false, "uint16_t", "ENUM"              },
        {"uint16_t_set",        &meta_check_field<std::uint16_t>,      column_type::set,       false, "uint16_t", "SET"               },
        {"uint16_t_json",       &meta_check_field<std::uint16_t>,      column_type::json,      false, "uint16_t", "JSON"              },
        {"uint16_t_decimal",    &meta_check_field<std::uint16_t>,      column_type::decimal,   false, "uint16_t", "DECIMAL"           },
        {"uint16_t_binary",     &meta_check_field<std::uint16_t>,      column_type::binary,    false, "uint16_t", "BINARY"            },
        {"uint16_t_varbinary",  &meta_check_field<std::uint16_t>,      column_type::varbinary, false, "uint16_t", "VARBINARY"         },
        {"uint16_t_blob",       &meta_check_field<std::uint16_t>,      column_type::blob,      false, "uint16_t", "BLOB"              },
        {"uint16_t_geometry",   &meta_check_field<std::uint16_t>,      column_type::geometry,  false, "uint16_t", "GEOMETRY"          },
        {"uint16_t_unknown",    &meta_check_field<std::uint16_t>,      column_type::unknown,   false, "uint16_t", "<unknown column type>"           },
        {"int32_t_intu",        &meta_check_field<std::int32_t>,       column_type::int_,      true,  "int32_t",  "INT UNSIGNED"      },
        {"int32_t_bigint",      &meta_check_field<std::int32_t>,       column_type::bigint,    false, "int32_t",  "BIGINT"            },
        {"int32_t_bigintu",     &meta_check_field<std::int32_t>,       column_type::bigint,    true,  "int32_t",  "BIGINT UNSIGNED"   },
        {"int32_t_bit",         &meta_check_field<std::int32_t>,       column_type::bit,       true,  "int32_t",  "BIT"               },
        {"int32_t_float",       &meta_check_field<std::int32_t>,       column_type::float_,    false, "int32_t",  "FLOAT"             },
        {"int32_t_double",      &meta_check_field<std::int32_t>,       column_type::double_,   false, "int32_t",  "DOUBLE"            },
        {"int32_t_date",        &meta_check_field<std::int32_t>,       column_type::date,      false, "int32_t",  "DATE"              },
        {"int32_t_datetime",    &meta_check_field<std::int32_t>,       column_type::datetime,  false, "int32_t",  "DATETIME"          },
        {"int32_t_timestamp",   &meta_check_field<std::int32_t>,       column_type::timestamp, false, "int32_t",  "TIMESTAMP"         },
        {"int32_t_time",        &meta_check_field<std::int32_t>,       column_type::time,      false, "int32_t",  "TIME"              },
        {"int32_t_char",        &meta_check_field<std::int32_t>,       column_type::char_,     false, "int32_t",  "CHAR"              },
        {"int32_t_varchar",     &meta_check_field<std::int32_t>,       column_type::varchar,   false, "int32_t",  "VARCHAR"           },
        {"int32_t_text",        &meta_check_field<std::int32_t>,       column_type::text,      false, "int32_t",  "TEXT"              },
        {"int32_t_enum",        &meta_check_field<std::int32_t>,       column_type::enum_,     false, "int32_t",  "ENUM"              },
        {"int32_t_set",         &meta_check_field<std::int32_t>,       column_type::set,       false, "int32_t",  "SET"               },
        {"int32_t_json",        &meta_check_field<std::int32_t>,       column_type::json,      false, "int32_t",  "JSON"              },
        {"int32_t_decimal",     &meta_check_field<std::int32_t>,       column_type::decimal,   false, "int32_t",  "DECIMAL"           },
        {"int32_t_binary",      &meta_check_field<std::int32_t>,       column_type::binary,    false, "int32_t",  "BINARY"            },
        {"int32_t_varbinary",   &meta_check_field<std::int32_t>,       column_type::varbinary, false, "int32_t",  "VARBINARY"         },
        {"int32_t_blob",        &meta_check_field<std::int32_t>,       column_type::blob,      false, "int32_t",  "BLOB"              },
        {"int32_t_geometry",    &meta_check_field<std::int32_t>,       column_type::geometry,  false, "int32_t",  "GEOMETRY"          },
        {"int32_t_unknown",     &meta_check_field<std::int32_t>,       column_type::unknown,   false, "int32_t",  "<unknown column type>"           },
        {"uint32_t_tinyint",    &meta_check_field<std::uint32_t>,      column_type::tinyint,   false, "uint32_t", "TINYINT"           },
        {"uint32_t_smallint",   &meta_check_field<std::uint32_t>,      column_type::smallint,  false, "uint32_t", "SMALLINT"          },
        {"uint32_t_mediumint",  &meta_check_field<std::uint32_t>,      column_type::mediumint, false, "uint32_t", "MEDIUMINT"         },
        {"uint32_t_int",        &meta_check_field<std::uint32_t>,      column_type::int_,      false, "uint32_t", "INT"               },
        {"uint32_t_bigint",     &meta_check_field<std::uint32_t>,      column_type::bigint,    false, "uint32_t", "BIGINT"            },
        {"uint32_t_bigintu",    &meta_check_field<std::uint32_t>,      column_type::bigint,    true,  "uint32_t", "BIGINT UNSIGNED"   },
        {"uint32_t_bit",        &meta_check_field<std::uint32_t>,      column_type::bit,       true,  "uint32_t", "BIT"               },
        {"uint32_t_float",      &meta_check_field<std::uint32_t>,      column_type::float_,    false, "uint32_t", "FLOAT"             },
        {"uint32_t_double",     &meta_check_field<std::uint32_t>,      column_type::double_,   false, "uint32_t", "DOUBLE"            },
        {"uint32_t_date",       &meta_check_field<std::uint32_t>,      column_type::date,      false, "uint32_t", "DATE"              },
        {"uint32_t_datetime",   &meta_check_field<std::uint32_t>,      column_type::datetime,  false, "uint32_t", "DATETIME"          },
        {"uint32_t_timestamp",  &meta_check_field<std::uint32_t>,      column_type::timestamp, false, "uint32_t", "TIMESTAMP"         },
        {"uint32_t_time",       &meta_check_field<std::uint32_t>,      column_type::time,      false, "uint32_t", "TIME"              },
        {"uint32_t_char",       &meta_check_field<std::uint32_t>,      column_type::char_,     false, "uint32_t", "CHAR"              },
        {"uint32_t_varchar",    &meta_check_field<std::uint32_t>,      column_type::varchar,   false, "uint32_t", "VARCHAR"           },
        {"uint32_t_text",       &meta_check_field<std::uint32_t>,      column_type::text,      false, "uint32_t", "TEXT"              },
        {"uint32_t_enum",       &meta_check_field<std::uint32_t>,      column_type::enum_,     false, "uint32_t", "ENUM"              },
        {"uint32_t_set",        &meta_check_field<std::uint32_t>,      column_type::set,       false, "uint32_t", "SET"               },
        {"uint32_t_json",       &meta_check_field<std::uint32_t>,      column_type::json,      false, "uint32_t", "JSON"              },
        {"uint32_t_decimal",    &meta_check_field<std::uint32_t>,      column_type::decimal,   false, "uint32_t", "DECIMAL"           },
        {"uint32_t_binary",     &meta_check_field<std::uint32_t>,      column_type::binary,    false, "uint32_t", "BINARY"            },
        {"uint32_t_varbinary",  &meta_check_field<std::uint32_t>,      column_type::varbinary, false, "uint32_t", "VARBINARY"         },
        {"uint32_t_blob",       &meta_check_field<std::uint32_t>,      column_type::blob,      false, "uint32_t", "BLOB"              },
        {"uint32_t_geometry",   &meta_check_field<std::uint32_t>,      column_type::geometry,  false, "uint32_t", "GEOMETRY"          },
        {"uint32_t_unknown",    &meta_check_field<std::uint32_t>,      column_type::unknown,   false, "uint32_t", "<unknown column type>"           },
        {"int64_t_bigintu",     &meta_check_field<std::int64_t>,       column_type::bigint,    true,  "int64_t",  "BIGINT UNSIGNED"   },
        {"int64_t_bit",         &meta_check_field<std::int64_t>,       column_type::bit,       true,  "int64_t",  "BIT"               },
        {"int64_t_float",       &meta_check_field<std::int64_t>,       column_type::float_,    false, "int64_t",  "FLOAT"             },
        {"int64_t_double",      &meta_check_field<std::int64_t>,       column_type::double_,   false, "int64_t",  "DOUBLE"            },
        {"int64_t_date",        &meta_check_field<std::int64_t>,       column_type::date,      false, "int64_t",  "DATE"              },
        {"int64_t_datetime",    &meta_check_field<std::int64_t>,       column_type::datetime,  false, "int64_t",  "DATETIME"          },
        {"int64_t_timestamp",   &meta_check_field<std::int64_t>,       column_type::timestamp, false, "int64_t",  "TIMESTAMP"         },
        {"int64_t_time",        &meta_check_field<std::int64_t>,       column_type::time,      false, "int64_t",  "TIME"              },
        {"int64_t_char",        &meta_check_field<std::int64_t>,       column_type::char_,     false, "int64_t",  "CHAR"              },
        {"int64_t_varchar",     &meta_check_field<std::int64_t>,       column_type::varchar,   false, "int64_t",  "VARCHAR"           },
        {"int64_t_text",        &meta_check_field<std::int64_t>,       column_type::text,      false, "int64_t",  "TEXT"              },
        {"int64_t_enum",        &meta_check_field<std::int64_t>,       column_type::enum_,     false, "int64_t",  "ENUM"              },
        {"int64_t_set",         &meta_check_field<std::int64_t>,       column_type::set,       false, "int64_t",  "SET"               },
        {"int64_t_json",        &meta_check_field<std::int64_t>,       column_type::json,      false, "int64_t",  "JSON"              },
        {"int64_t_decimal",     &meta_check_field<std::int64_t>,       column_type::decimal,   false, "int64_t",  "DECIMAL"           },
        {"int64_t_binary",      &meta_check_field<std::int64_t>,       column_type::binary,    false, "int64_t",  "BINARY"            },
        {"int64_t_varbinary",   &meta_check_field<std::int64_t>,       column_type::varbinary, false, "int64_t",  "VARBINARY"         },
        {"int64_t_blob",        &meta_check_field<std::int64_t>,       column_type::blob,      false, "int64_t",  "BLOB"              },
        {"int64_t_geometry",    &meta_check_field<std::int64_t>,       column_type::geometry,  false, "int64_t",  "GEOMETRY"          },
        {"int64_t_unknown",     &meta_check_field<std::int64_t>,       column_type::unknown,   false, "int64_t",  "<unknown column type>"           },
        {"uint64_t_tinyint",    &meta_check_field<std::uint64_t>,      column_type::tinyint,   false, "uint64_t", "TINYINT"           },
        {"uint64_t_smallint",   &meta_check_field<std::uint64_t>,      column_type::smallint,  false, "uint64_t", "SMALLINT"          },
        {"uint64_t_mediumint",  &meta_check_field<std::uint64_t>,      column_type::mediumint, false, "uint64_t", "MEDIUMINT"         },
        {"uint64_t_int",        &meta_check_field<std::uint64_t>,      column_type::int_,      false, "uint64_t", "INT"               },
        {"uint64_t_bigint",     &meta_check_field<std::uint64_t>,      column_type::bigint,    false, "uint64_t", "BIGINT"            },
        {"uint64_t_float",      &meta_check_field<std::uint64_t>,      column_type::float_,    false, "uint64_t", "FLOAT"             },
        {"uint64_t_double",     &meta_check_field<std::uint64_t>,      column_type::double_,   false, "uint64_t", "DOUBLE"            },
        {"uint64_t_date",       &meta_check_field<std::uint64_t>,      column_type::date,      false, "uint64_t", "DATE"              },
        {"uint64_t_datetime",   &meta_check_field<std::uint64_t>,      column_type::datetime,  false, "uint64_t", "DATETIME"          },
        {"uint64_t_timestamp",  &meta_check_field<std::uint64_t>,      column_type::timestamp, false, "uint64_t", "TIMESTAMP"         },
        {"uint64_t_time",       &meta_check_field<std::uint64_t>,      column_type::time,      false, "uint64_t", "TIME"              },
        {"uint64_t_char",       &meta_check_field<std::uint64_t>,      column_type::char_,     false, "uint64_t", "CHAR"              },
        {"uint64_t_varchar",    &meta_check_field<std::uint64_t>,      column_type::varchar,   false, "uint64_t", "VARCHAR"           },
        {"uint64_t_text",       &meta_check_field<std::uint64_t>,      column_type::text,      false, "uint64_t", "TEXT"              },
        {"uint64_t_enum",       &meta_check_field<std::uint64_t>,      column_type::enum_,     false, "uint64_t", "ENUM"              },
        {"uint64_t_set",        &meta_check_field<std::uint64_t>,      column_type::set,       false, "uint64_t", "SET"               },
        {"uint64_t_json",       &meta_check_field<std::uint64_t>,      column_type::json,      false, "uint64_t", "JSON"              },
        {"uint64_t_decimal",    &meta_check_field<std::uint64_t>,      column_type::decimal,   false, "uint64_t", "DECIMAL"           },
        {"uint64_t_binary",     &meta_check_field<std::uint64_t>,      column_type::binary,    false, "uint64_t", "BINARY"            },
        {"uint64_t_varbinary",  &meta_check_field<std::uint64_t>,      column_type::varbinary, false, "uint64_t", "VARBINARY"         },
        {"uint64_t_blob",       &meta_check_field<std::uint64_t>,      column_type::blob,      false, "uint64_t", "BLOB"              },
        {"uint64_t_geometry",   &meta_check_field<std::uint64_t>,      column_type::geometry,  false, "uint64_t", "GEOMETRY"          },
        {"uint64_t_unknown",    &meta_check_field<std::uint64_t>,      column_type::unknown,   false, "uint64_t", "<unknown column type>"           },
        {"bool_tinyintu",       &meta_check_field<bool>,               column_type::tinyint,   true,  "bool",     "TINYINT UNSIGNED"  },
        {"bool_smallint",       &meta_check_field<bool>,               column_type::smallint,  false, "bool",     "SMALLINT"          },
        {"bool_smallintu",      &meta_check_field<bool>,               column_type::smallint,  true,  "bool",     "SMALLINT UNSIGNED" },
        {"bool_mediumint",      &meta_check_field<bool>,               column_type::mediumint, false, "bool",     "MEDIUMINT"         },
        {"bool_mediumintu",     &meta_check_field<bool>,               column_type::mediumint, true,  "bool",     "MEDIUMINT UNSIGNED"},
        {"bool_int",            &meta_check_field<bool>,               column_type::int_,      false, "bool",     "INT"               },
        {"bool_intu",           &meta_check_field<bool>,               column_type::int_,      true,  "bool",     "INT UNSIGNED"      },
        {"bool_bigint",         &meta_check_field<bool>,               column_type::bigint,    false, "bool",     "BIGINT"            },
        {"bool_bigintu",        &meta_check_field<bool>,               column_type::bigint,    true,  "bool",     "BIGINT UNSIGNED"   },
        {"bool_year",           &meta_check_field<bool>,               column_type::year,      true,  "bool",     "YEAR"              },
        {"bool_bit",            &meta_check_field<bool>,               column_type::bit,       true,  "bool",     "BIT"               },
        {"bool_float",          &meta_check_field<bool>,               column_type::float_,    false, "bool",     "FLOAT"             },
        {"bool_double",         &meta_check_field<bool>,               column_type::double_,   false, "bool",     "DOUBLE"            },
        {"bool_date",           &meta_check_field<bool>,               column_type::date,      false, "bool",     "DATE"              },
        {"bool_datetime",       &meta_check_field<bool>,               column_type::datetime,  false, "bool",     "DATETIME"          },
        {"bool_timestamp",      &meta_check_field<bool>,               column_type::timestamp, false, "bool",     "TIMESTAMP"         },
        {"bool_time",           &meta_check_field<bool>,               column_type::time,      false, "bool",     "TIME"              },
        {"bool_char",           &meta_check_field<bool>,               column_type::char_,     false, "bool",     "CHAR"              },
        {"bool_varchar",        &meta_check_field<bool>,               column_type::varchar,   false, "bool",     "VARCHAR"           },
        {"bool_text",           &meta_check_field<bool>,               column_type::text,      false, "bool",     "TEXT"              },
        {"bool_enum",           &meta_check_field<bool>,               column_type::enum_,     false, "bool",     "ENUM"              },
        {"bool_set",            &meta_check_field<bool>,               column_type::set,       false, "bool",     "SET"               },
        {"bool_json",           &meta_check_field<bool>,               column_type::json,      false, "bool",     "JSON"              },
        {"bool_decimal",        &meta_check_field<bool>,               column_type::decimal,   false, "bool",     "DECIMAL"           },
        {"bool_binary",         &meta_check_field<bool>,               column_type::binary,    false, "bool",     "BINARY"            },
        {"bool_varbinary",      &meta_check_field<bool>,               column_type::varbinary, false, "bool",     "VARBINARY"         },
        {"bool_blob",           &meta_check_field<bool>,               column_type::blob,      false, "bool",     "BLOB"              },
        {"bool_geometry",       &meta_check_field<bool>,               column_type::geometry,  false, "bool",     "GEOMETRY"          },
        {"bool_unknown",        &meta_check_field<bool>,               column_type::unknown,   false, "bool",     "<unknown column type>"           },
        {"float_tinyint",       &meta_check_field<float>,              column_type::tinyint,   false, "float",    "TINYINT"           },
        {"float_tinyintu",      &meta_check_field<float>,              column_type::tinyint,   true,  "float",    "TINYINT UNSIGNED"  },
        {"float_smallint",      &meta_check_field<float>,              column_type::smallint,  false, "float",    "SMALLINT"          },
        {"float_smallintu",     &meta_check_field<float>,              column_type::smallint,  true,  "float",    "SMALLINT UNSIGNED" },
        {"float_mediumint",     &meta_check_field<float>,              column_type::mediumint, false, "float",    "MEDIUMINT"         },
        {"float_mediumintu",    &meta_check_field<float>,              column_type::mediumint, true,  "float",    "MEDIUMINT UNSIGNED"},
        {"float_int",           &meta_check_field<float>,              column_type::int_,      false, "float",    "INT"               },
        {"float_intu",          &meta_check_field<float>,              column_type::int_,      true,  "float",    "INT UNSIGNED"      },
        {"float_bigint",        &meta_check_field<float>,              column_type::bigint,    false, "float",    "BIGINT"            },
        {"float_bigintu",       &meta_check_field<float>,              column_type::bigint,    true,  "float",    "BIGINT UNSIGNED"   },
        {"float_year",          &meta_check_field<float>,              column_type::year,      true,  "float",    "YEAR"              },
        {"float_bit",           &meta_check_field<float>,              column_type::bit,       true,  "float",    "BIT"               },
        {"float_double",        &meta_check_field<float>,              column_type::double_,   false, "float",    "DOUBLE"            },
        {"float_date",          &meta_check_field<float>,              column_type::date,      false, "float",    "DATE"              },
        {"float_datetime",      &meta_check_field<float>,              column_type::datetime,  false, "float",    "DATETIME"          },
        {"float_timestamp",     &meta_check_field<float>,              column_type::timestamp, false, "float",    "TIMESTAMP"         },
        {"float_time",          &meta_check_field<float>,              column_type::time,      false, "float",    "TIME"              },
        {"float_char",          &meta_check_field<float>,              column_type::char_,     false, "float",    "CHAR"              },
        {"float_varchar",       &meta_check_field<float>,              column_type::varchar,   false, "float",    "VARCHAR"           },
        {"float_text",          &meta_check_field<float>,              column_type::text,      false, "float",    "TEXT"              },
        {"float_enum",          &meta_check_field<float>,              column_type::enum_,     false, "float",    "ENUM"              },
        {"float_set",           &meta_check_field<float>,              column_type::set,       false, "float",    "SET"               },
        {"float_json",          &meta_check_field<float>,              column_type::json,      false, "float",    "JSON"              },
        {"float_decimal",       &meta_check_field<float>,              column_type::decimal,   false, "float",    "DECIMAL"           },
        {"float_binary",        &meta_check_field<float>,              column_type::binary,    false, "float",    "BINARY"            },
        {"float_varbinary",     &meta_check_field<float>,              column_type::varbinary, false, "float",    "VARBINARY"         },
        {"float_blob",          &meta_check_field<float>,              column_type::blob,      false, "float",    "BLOB"              },
        {"float_geometry",      &meta_check_field<float>,              column_type::geometry,  false, "float",    "GEOMETRY"          },
        {"float_unknown",       &meta_check_field<float>,              column_type::unknown,   false, "float",    "<unknown column type>"           },
        {"double_tinyint",      &meta_check_field<double>,             column_type::tinyint,   false, "double",   "TINYINT"           },
        {"double_tinyintu",     &meta_check_field<double>,             column_type::tinyint,   true,  "double",   "TINYINT UNSIGNED"  },
        {"double_smallint",     &meta_check_field<double>,             column_type::smallint,  false, "double",   "SMALLINT"          },
        {"double_smallintu",    &meta_check_field<double>,             column_type::smallint,  true,  "double",   "SMALLINT UNSIGNED" },
        {"double_mediumint",    &meta_check_field<double>,             column_type::mediumint, false, "double",   "MEDIUMINT"         },
        {"double_mediumintu",   &meta_check_field<double>,             column_type::mediumint, true,  "double",   "MEDIUMINT UNSIGNED"},
        {"double_int",          &meta_check_field<double>,             column_type::int_,      false, "double",   "INT"               },
        {"double_intu",         &meta_check_field<double>,             column_type::int_,      true,  "double",   "INT UNSIGNED"      },
        {"double_bigint",       &meta_check_field<double>,             column_type::bigint,    false, "double",   "BIGINT"            },
        {"double_bigintu",      &meta_check_field<double>,             column_type::bigint,    true,  "double",   "BIGINT UNSIGNED"   },
        {"double_year",         &meta_check_field<double>,             column_type::year,      true,  "double",   "YEAR"              },
        {"double_bit",          &meta_check_field<double>,             column_type::bit,       true,  "double",   "BIT"               },
        {"double_date",         &meta_check_field<double>,             column_type::date,      false, "double",   "DATE"              },
        {"double_datetime",     &meta_check_field<double>,             column_type::datetime,  false, "double",   "DATETIME"          },
        {"double_timestamp",    &meta_check_field<double>,             column_type::timestamp, false, "double",   "TIMESTAMP"         },
        {"double_time",         &meta_check_field<double>,             column_type::time,      false, "double",   "TIME"              },
        {"double_char",         &meta_check_field<double>,             column_type::char_,     false, "double",   "CHAR"              },
        {"double_varchar",      &meta_check_field<double>,             column_type::varchar,   false, "double",   "VARCHAR"           },
        {"double_text",         &meta_check_field<double>,             column_type::text,      false, "double",   "TEXT"              },
        {"double_enum",         &meta_check_field<double>,             column_type::enum_,     false, "double",   "ENUM"              },
        {"double_set",          &meta_check_field<double>,             column_type::set,       false, "double",   "SET"               },
        {"double_json",         &meta_check_field<double>,             column_type::json,      false, "double",   "JSON"              },
        {"double_decimal",      &meta_check_field<double>,             column_type::decimal,   false, "double",   "DECIMAL"           },
        {"double_binary",       &meta_check_field<double>,             column_type::binary,    false, "double",   "BINARY"            },
        {"double_varbinary",    &meta_check_field<double>,             column_type::varbinary, false, "double",   "VARBINARY"         },
        {"double_blob",         &meta_check_field<double>,             column_type::blob,      false, "double",   "BLOB"              },
        {"double_geometry",     &meta_check_field<double>,             column_type::geometry,  false, "double",   "GEOMETRY"          },
        {"double_unknown",      &meta_check_field<double>,             column_type::unknown,   false, "double",   "<unknown column type>"           },
        {"date_tinyint",        &meta_check_field<date>,               column_type::tinyint,   false, "date",     "TINYINT"           },
        {"date_tinyintu",       &meta_check_field<date>,               column_type::tinyint,   true,  "date",     "TINYINT UNSIGNED"  },
        {"date_smallint",       &meta_check_field<date>,               column_type::smallint,  false, "date",     "SMALLINT"          },
        {"date_smallintu",      &meta_check_field<date>,               column_type::smallint,  true,  "date",     "SMALLINT UNSIGNED" },
        {"date_mediumint",      &meta_check_field<date>,               column_type::mediumint, false, "date",     "MEDIUMINT"         },
        {"date_mediumintu",     &meta_check_field<date>,               column_type::mediumint, true,  "date",     "MEDIUMINT UNSIGNED"},
        {"date_int",            &meta_check_field<date>,               column_type::int_,      false, "date",     "INT"               },
        {"date_intu",           &meta_check_field<date>,               column_type::int_,      true,  "date",     "INT UNSIGNED"      },
        {"date_bigint",         &meta_check_field<date>,               column_type::bigint,    false, "date",     "BIGINT"            },
        {"date_bigintu",        &meta_check_field<date>,               column_type::bigint,    true,  "date",     "BIGINT UNSIGNED"   },
        {"date_year",           &meta_check_field<date>,               column_type::year,      true,  "date",     "YEAR"              },
        {"date_bit",            &meta_check_field<date>,               column_type::bit,       true,  "date",     "BIT"               },
        {"date_float",          &meta_check_field<date>,               column_type::float_,    false, "date",     "FLOAT"             },
        {"date_double",         &meta_check_field<date>,               column_type::double_,   false, "date",     "DOUBLE"            },
        {"date_datetime",       &meta_check_field<date>,               column_type::datetime,  false, "date",     "DATETIME"          },
        {"date_timestamp",      &meta_check_field<date>,               column_type::timestamp, false, "date",     "TIMESTAMP"         },
        {"date_time",           &meta_check_field<date>,               column_type::time,      false, "date",     "TIME"              },
        {"date_char",           &meta_check_field<date>,               column_type::char_,     false, "date",     "CHAR"              },
        {"date_varchar",        &meta_check_field<date>,               column_type::varchar,   false, "date",     "VARCHAR"           },
        {"date_text",           &meta_check_field<date>,               column_type::text,      false, "date",     "TEXT"              },
        {"date_enum",           &meta_check_field<date>,               column_type::enum_,     false, "date",     "ENUM"              },
        {"date_set",            &meta_check_field<date>,               column_type::set,       false, "date",     "SET"               },
        {"date_json",           &meta_check_field<date>,               column_type::json,      false, "date",     "JSON"              },
        {"date_decimal",        &meta_check_field<date>,               column_type::decimal,   false, "date",     "DECIMAL"           },
        {"date_binary",         &meta_check_field<date>,               column_type::binary,    false, "date",     "BINARY"            },
        {"date_varbinary",      &meta_check_field<date>,               column_type::varbinary, false, "date",     "VARBINARY"         },
        {"date_blob",           &meta_check_field<date>,               column_type::blob,      false, "date",     "BLOB"              },
        {"date_geometry",       &meta_check_field<date>,               column_type::geometry,  false, "date",     "GEOMETRY"          },
        {"date_unknown",        &meta_check_field<date>,               column_type::unknown,   false, "date",     "<unknown column type>"           },
        {"datetime_tinyint",    &meta_check_field<datetime>,           column_type::tinyint,   false, "datetime", "TINYINT"           },
        {"datetime_tinyintu",   &meta_check_field<datetime>,           column_type::tinyint,   true,  "datetime", "TINYINT UNSIGNED"  },
        {"datetime_smallint",   &meta_check_field<datetime>,           column_type::smallint,  false, "datetime", "SMALLINT"          },
        {"datetime_smallintu",  &meta_check_field<datetime>,           column_type::smallint,  true,  "datetime", "SMALLINT UNSIGNED" },
        {"datetime_mediumint",  &meta_check_field<datetime>,           column_type::mediumint, false, "datetime", "MEDIUMINT"         },
        {"datetime_mediumintu", &meta_check_field<datetime>,           column_type::mediumint, true,  "datetime", "MEDIUMINT UNSIGNED"},
        {"datetime_int",        &meta_check_field<datetime>,           column_type::int_,      false, "datetime", "INT"               },
        {"datetime_intu",       &meta_check_field<datetime>,           column_type::int_,      true,  "datetime", "INT UNSIGNED"      },
        {"datetime_bigint",     &meta_check_field<datetime>,           column_type::bigint,    false, "datetime", "BIGINT"            },
        {"datetime_bigintu",    &meta_check_field<datetime>,           column_type::bigint,    true,  "datetime", "BIGINT UNSIGNED"   },
        {"datetime_year",       &meta_check_field<datetime>,           column_type::year,      true,  "datetime", "YEAR"              },
        {"datetime_bit",        &meta_check_field<datetime>,           column_type::bit,       true,  "datetime", "BIT"               },
        {"datetime_float",      &meta_check_field<datetime>,           column_type::float_,    false, "datetime", "FLOAT"             },
        {"datetime_double",     &meta_check_field<datetime>,           column_type::double_,   false, "datetime", "DOUBLE"            },
        {"datetime_date",       &meta_check_field<datetime>,           column_type::date,      false, "datetime", "DATE"              },
        {"datetime_time",       &meta_check_field<datetime>,           column_type::time,      false, "datetime", "TIME"              },
        {"datetime_char",       &meta_check_field<datetime>,           column_type::char_,     false, "datetime", "CHAR"              },
        {"datetime_varchar",    &meta_check_field<datetime>,           column_type::varchar,   false, "datetime", "VARCHAR"           },
        {"datetime_text",       &meta_check_field<datetime>,           column_type::text,      false, "datetime", "TEXT"              },
        {"datetime_enum",       &meta_check_field<datetime>,           column_type::enum_,     false, "datetime", "ENUM"              },
        {"datetime_set",        &meta_check_field<datetime>,           column_type::set,       false, "datetime", "SET"               },
        {"datetime_json",       &meta_check_field<datetime>,           column_type::json,      false, "datetime", "JSON"              },
        {"datetime_decimal",    &meta_check_field<datetime>,           column_type::decimal,   false, "datetime", "DECIMAL"           },
        {"datetime_binary",     &meta_check_field<datetime>,           column_type::binary,    false, "datetime", "BINARY"            },
        {"datetime_varbinary",  &meta_check_field<datetime>,           column_type::varbinary, false, "datetime", "VARBINARY"         },
        {"datetime_blob",       &meta_check_field<datetime>,           column_type::blob,      false, "datetime", "BLOB"              },
        {"datetime_geometry",   &meta_check_field<datetime>,           column_type::geometry,  false, "datetime", "GEOMETRY"          },
        {"datetime_unknown",    &meta_check_field<datetime>,           column_type::unknown,   false, "datetime", "<unknown column type>"           },
        {"time_tinyint",        &meta_check_field<boost::mysql::time>, column_type::tinyint,   false, "time",     "TINYINT"           },
        {"time_tinyintu",       &meta_check_field<boost::mysql::time>, column_type::tinyint,   true,  "time",     "TINYINT UNSIGNED"  },
        {"time_smallint",       &meta_check_field<boost::mysql::time>, column_type::smallint,  false, "time",     "SMALLINT"          },
        {"time_smallintu",      &meta_check_field<boost::mysql::time>, column_type::smallint,  true,  "time",     "SMALLINT UNSIGNED" },
        {"time_mediumint",      &meta_check_field<boost::mysql::time>, column_type::mediumint, false, "time",     "MEDIUMINT"         },
        {"time_mediumintu",     &meta_check_field<boost::mysql::time>, column_type::mediumint, true,  "time",     "MEDIUMINT UNSIGNED"},
        {"time_int",            &meta_check_field<boost::mysql::time>, column_type::int_,      false, "time",     "INT"               },
        {"time_intu",           &meta_check_field<boost::mysql::time>, column_type::int_,      true,  "time",     "INT UNSIGNED"      },
        {"time_bigint",         &meta_check_field<boost::mysql::time>, column_type::bigint,    false, "time",     "BIGINT"            },
        {"time_bigintu",        &meta_check_field<boost::mysql::time>, column_type::bigint,    true,  "time",     "BIGINT UNSIGNED"   },
        {"time_year",           &meta_check_field<boost::mysql::time>, column_type::year,      true,  "time",     "YEAR"              },
        {"time_bit",            &meta_check_field<boost::mysql::time>, column_type::bit,       true,  "time",     "BIT"               },
        {"time_float",          &meta_check_field<boost::mysql::time>, column_type::float_,    false, "time",     "FLOAT"             },
        {"time_double",         &meta_check_field<boost::mysql::time>, column_type::double_,   false, "time",     "DOUBLE"            },
        {"time_date",           &meta_check_field<boost::mysql::time>, column_type::date,      false, "time",     "DATE"              },
        {"time_datetime",       &meta_check_field<boost::mysql::time>, column_type::datetime,  false, "time",     "DATETIME"          },
        {"time_timestamp",      &meta_check_field<boost::mysql::time>, column_type::timestamp, false, "time",     "TIMESTAMP"         },
        {"time_char",           &meta_check_field<boost::mysql::time>, column_type::char_,     false, "time",     "CHAR"              },
        {"time_varchar",        &meta_check_field<boost::mysql::time>, column_type::varchar,   false, "time",     "VARCHAR"           },
        {"time_text",           &meta_check_field<boost::mysql::time>, column_type::text,      false, "time",     "TEXT"              },
        {"time_enum",           &meta_check_field<boost::mysql::time>, column_type::enum_,     false, "time",     "ENUM"              },
        {"time_set",            &meta_check_field<boost::mysql::time>, column_type::set,       false, "time",     "SET"               },
        {"time_json",           &meta_check_field<boost::mysql::time>, column_type::json,      false, "time",     "JSON"              },
        {"time_decimal",        &meta_check_field<boost::mysql::time>, column_type::decimal,   false, "time",     "DECIMAL"           },
        {"time_binary",         &meta_check_field<boost::mysql::time>, column_type::binary,    false, "time",     "BINARY"            },
        {"time_varbinary",      &meta_check_field<boost::mysql::time>, column_type::varbinary, false, "time",     "VARBINARY"         },
        {"time_blob",           &meta_check_field<boost::mysql::time>, column_type::blob,      false, "time",     "BLOB"              },
        {"time_geometry",       &meta_check_field<boost::mysql::time>, column_type::geometry,  false, "time",     "GEOMETRY"          },
        {"time_unknown",        &meta_check_field<boost::mysql::time>, column_type::unknown,   false, "time",     "<unknown column type>"           },
        {"string_tinyint",      &meta_check_field<std::string>,        column_type::tinyint,   false, "string",   "TINYINT"           },
        {"string_tinyintu",     &meta_check_field<std::string>,        column_type::tinyint,   true,  "string",   "TINYINT UNSIGNED"  },
        {"string_smallint",     &meta_check_field<std::string>,        column_type::smallint,  false, "string",   "SMALLINT"          },
        {"string_smallintu",    &meta_check_field<std::string>,        column_type::smallint,  true,  "string",   "SMALLINT UNSIGNED" },
        {"string_mediumint",    &meta_check_field<std::string>,        column_type::mediumint, false, "string",   "MEDIUMINT"         },
        {"string_mediumintu",   &meta_check_field<std::string>,        column_type::mediumint, true,  "string",   "MEDIUMINT UNSIGNED"},
        {"string_int",          &meta_check_field<std::string>,        column_type::int_,      false, "string",   "INT"               },
        {"string_intu",         &meta_check_field<std::string>,        column_type::int_,      true,  "string",   "INT UNSIGNED"      },
        {"string_bigint",       &meta_check_field<std::string>,        column_type::bigint,    false, "string",   "BIGINT"            },
        {"string_bigintu",      &meta_check_field<std::string>,        column_type::bigint,    true,  "string",   "BIGINT UNSIGNED"   },
        {"string_year",         &meta_check_field<std::string>,        column_type::year,      true,  "string",   "YEAR"              },
        {"string_bit",          &meta_check_field<std::string>,        column_type::bit,       true,  "string",   "BIT"               },
        {"string_float",        &meta_check_field<std::string>,        column_type::float_,    false, "string",   "FLOAT"             },
        {"string_double",       &meta_check_field<std::string>,        column_type::double_,   false, "string",   "DOUBLE"            },
        {"string_date",         &meta_check_field<std::string>,        column_type::date,      false, "string",   "DATE"              },
        {"string_datetime",     &meta_check_field<std::string>,        column_type::datetime,  false, "string",   "DATETIME"          },
        {"string_timestamp",    &meta_check_field<std::string>,        column_type::timestamp, false, "string",   "TIMESTAMP"         },
        {"string_time",         &meta_check_field<std::string>,        column_type::time,      false, "string",   "TIME"              },
        {"string_binary",       &meta_check_field<std::string>,        column_type::binary,    false, "string",   "BINARY"            },
        {"string_varbinary",    &meta_check_field<std::string>,        column_type::varbinary, false, "string",   "VARBINARY"         },
        {"string_blob",         &meta_check_field<std::string>,        column_type::blob,      false, "string",   "BLOB"              },
        {"string_geometry",     &meta_check_field<std::string>,        column_type::geometry,  false, "string",   "GEOMETRY"          },
        {"string_unknown",      &meta_check_field<std::string>,        column_type::unknown,   false, "string",   "<unknown column type>"           },
        {"blob_tinyint",        &meta_check_field<blob>,               column_type::tinyint,   false, "blob",     "TINYINT"           },
        {"blob_tinyintu",       &meta_check_field<blob>,               column_type::tinyint,   true,  "blob",     "TINYINT UNSIGNED"  },
        {"blob_smallint",       &meta_check_field<blob>,               column_type::smallint,  false, "blob",     "SMALLINT"          },
        {"blob_smallintu",      &meta_check_field<blob>,               column_type::smallint,  true,  "blob",     "SMALLINT UNSIGNED" },
        {"blob_mediumint",      &meta_check_field<blob>,               column_type::mediumint, false, "blob",     "MEDIUMINT"         },
        {"blob_mediumintu",     &meta_check_field<blob>,               column_type::mediumint, true,  "blob",     "MEDIUMINT UNSIGNED"},
        {"blob_int",            &meta_check_field<blob>,               column_type::int_,      false, "blob",     "INT"               },
        {"blob_intu",           &meta_check_field<blob>,               column_type::int_,      true,  "blob",     "INT UNSIGNED"      },
        {"blob_bigint",         &meta_check_field<blob>,               column_type::bigint,    false, "blob",     "BIGINT"            },
        {"blob_bigintu",        &meta_check_field<blob>,               column_type::bigint,    true,  "blob",     "BIGINT UNSIGNED"   },
        {"blob_year",           &meta_check_field<blob>,               column_type::year,      true,  "blob",     "YEAR"              },
        {"blob_bit",            &meta_check_field<blob>,               column_type::bit,       true,  "blob",     "BIT"               },
        {"blob_float",          &meta_check_field<blob>,               column_type::float_,    false, "blob",     "FLOAT"             },
        {"blob_double",         &meta_check_field<blob>,               column_type::double_,   false, "blob",     "DOUBLE"            },
        {"blob_date",           &meta_check_field<blob>,               column_type::date,      false, "blob",     "DATE"              },
        {"blob_datetime",       &meta_check_field<blob>,               column_type::datetime,  false, "blob",     "DATETIME"          },
        {"blob_timestamp",      &meta_check_field<blob>,               column_type::timestamp, false, "blob",     "TIMESTAMP"         },
        {"blob_time",           &meta_check_field<blob>,               column_type::time,      false, "blob",     "TIME"              },
        {"blob_char",           &meta_check_field<blob>,               column_type::char_,     false, "blob",     "CHAR"              },
        {"blob_varchar",        &meta_check_field<blob>,               column_type::varchar,   false, "blob",     "VARCHAR"           },
        {"blob_text",           &meta_check_field<blob>,               column_type::text,      false, "blob",     "TEXT"              },
        {"blob_enum",           &meta_check_field<blob>,               column_type::enum_,     false, "blob",     "ENUM"              },
        {"blob_set",            &meta_check_field<blob>,               column_type::set,       false, "blob",     "SET"               },
        {"blob_json",           &meta_check_field<blob>,               column_type::json,      false, "blob",     "JSON"              },
        {"blob_decimal",        &meta_check_field<blob>,               column_type::decimal,   false, "blob",     "DECIMAL"           },
  // clang-format on
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            auto meta = meta_builder().type(tc.type).unsigned_flag(tc.is_unsigned).nullable(false).build();
            const std::size_t pos_map[] = {0};
            meta_check_context ctx(&meta, nullptr, pos_map);

            tc.check_fn(ctx);
            diagnostics diag;
            auto err = ctx.check_errors(diag);

            BOOST_TEST(err == client_errc::metadata_check_failed);
            std::string msg = "Incompatible types for field in position 0: C++ type '" +
                              std::string(tc.cpp_name) + "' is not compatible with DB type '" +
                              std::string(tc.db_name) + "'";
            BOOST_TEST(diag.client_message() == msg);
        }
    }
}
// meta_check:
//     all valid
//     all invalid: error message matches
//     optionality
// parse:
//     make parse errors non-fatal (?)
//     check for all valid types
//     check for all invalid types
BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

}  // namespace
