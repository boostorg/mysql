//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FIELD_TYPE_HPP
#define BOOST_MYSQL_FIELD_TYPE_HPP

#include <cstdint>
#include <ostream>

namespace boost {
namespace mysql {

/**
 * \ingroup resultsets
 * \brief Represents the type of a MySQL field.
 * \details Unless otherwise noted, the names in this enumeration
 * directly correspond to the names of the types you would use in
 * a CREATE TABLE statement to create a field of this type
 * (e.g. VARCHAR corresponds to field_type::varchar).
 */
enum class field_type
{
    tinyint,      ///< TINYINT (signed and unsigned).
    smallint,     ///< SMALLINT (signed and unsigned).
    mediumint,    ///< MEDIUMINT (signed and unsigned).
    int_,         ///< Plain INT (signed and unsigned).
    bigint,       ///< BIGINT (signed and unsigned).
    float_,       ///< FLOAT (warning: FLOAT(p) where p >= 24 creates a DOUBLE column).
    double_,      ///< DOUBLE
    decimal,      ///< DECIMAL
    bit,          ///< BIT
    year,         ///< YEAR
    time,         ///< TIME
    date,         ///< DATE
    datetime,     ///< DATETIME
    timestamp,    ///< TIMESTAMP
    char_,        ///< CHAR (any length)
    varchar,      ///< VARCHAR (any length)
    binary,       ///< BINARY (any length)
    varbinary,    ///< VARBINARY (any length)
    text,         ///< TINYTEXT, TEXT, MEDIUMTEXT and LONGTEXT
    blob,         ///< TINYBLOB, BLOB, MEDIUMBLOB and LONGBLOB
    enum_,        ///< ENUM
    set,          ///< SET
    geometry,     ///< GEOMETRY

    unknown,      ///< None of the known types; maybe a new MySQL type we have no knowledge of.
    _not_computed,
};

/**
 * \relates field_type
 * \brief Streams a boost::mysql::field_type.
 */
inline std::ostream& operator<<(std::ostream& os, field_type t)
{
    switch (t)
    {
    case field_type::tinyint: return os << "tinyint";
    case field_type::smallint: return os << "smallint";
    case field_type::mediumint: return os << "mediumint";
    case field_type::int_: return os << "int_";
    case field_type::bigint: return os << "bigint";
    case field_type::float_: return os << "float_";
    case field_type::double_: return os << "double_";
    case field_type::decimal: return os << "decimal";
    case field_type::bit: return os << "bit";
    case field_type::year: return os << "year";
    case field_type::time: return os << "time";
    case field_type::date: return os << "date";
    case field_type::datetime: return os << "datetime";
    case field_type::timestamp: return os << "timestamp";
    case field_type::char_: return os << "char_";
    case field_type::varchar: return os << "varchar";
    case field_type::binary: return os << "binary";
    case field_type::varbinary: return os << "varbinary";
    case field_type::text: return os << "text";
    case field_type::blob: return os << "blob";
    case field_type::enum_: return os << "enum_";
    case field_type::set: return os << "set";
    case field_type::geometry: return os << "geometry";
    default: return os << "<unknown field type>";
    }
}

} // mysql
} // boost



#endif
