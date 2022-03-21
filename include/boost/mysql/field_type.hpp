//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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
 * \brief Represents the type of a MySQL field.
 * \details Unless otherwise noted, the names in this enumeration
 * directly correspond to the names of the types you would use in
 * a `CREATE TABLE` statement to create a field of this type
 * (e.g. `__VARCHAR__` corresponds to `field_type::varchar`).
 */
enum class field_type
{
    tinyint,      ///< `__TINYINT__` (signed and unsigned).
    smallint,     ///< `__SMALLINT__` (signed and unsigned).
    mediumint,    ///< `__MEDIUMINT__` (signed and unsigned).
    int_,         ///< `__INT__` (signed and unsigned).
    bigint,       ///< `__BIGINT__` (signed and unsigned).
    float_,       ///< `__FLOAT__` (warning: FLOAT(p) where p >= 24 creates a DOUBLE column).
    double_,      ///< `__DOUBLE__`
    decimal,      ///< `__DECIMAL__`
    bit,          ///< `__BIT__`
    year,         ///< `__YEAR__`
    time,         ///< `__TIME__`
    date,         ///< `__DATE__`
    datetime,     ///< `__DATETIME__`
    timestamp,    ///< `__TIMESTAMP__`
    char_,        ///< `__CHAR__` (any length)
    varchar,      ///< `__VARCHAR__` (any length)
    binary,       ///< `__BINARY__` (any length)
    varbinary,    ///< `__VARBINARY__` (any length)
    text,         ///< `__TEXT__` types (`TINYTEXT`, `MEDIUMTEXT`, `TEXT` and `LONGTEXT`)
    blob,         ///< `__BLOB__` types (`TINYBLOB`, `MEDIUMBLOB`, `BLOB` and `LONGBLOB`)
    enum_,        ///< `__ENUM__`
    set,          ///< `__SET__`
    geometry,     ///< `__GEOMETRY__`

    unknown,      ///< None of the known types; maybe a new MySQL type we have no knowledge of.
#ifndef BOOST_MYSQL_DOXYGEN
    _not_computed,
#endif
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
