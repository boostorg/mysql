#ifndef MYSQL_ASIO_FIELD_TYPE_HPP
#define MYSQL_ASIO_FIELD_TYPE_HPP

#include <cstdint>

namespace boost {
namespace mysql {

/**
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

} // mysql
} // boost



#endif
