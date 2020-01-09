#ifndef MYSQL_ASIO_FIELD_TYPE_HPP
#define MYSQL_ASIO_FIELD_TYPE_HPP

#include <cstdint>

namespace mysql
{

enum class field_type
{
	tinyint,
	smallint,
	mediumint,
	int_,
	bigint,
	float_,
	double_,
	decimal,
	bit,
	year,
	time,
	date,
	datetime,
	timestamp,
	char_,
	varchar,
	binary,
	varbinary,
	text, // any TEXT size
	blob, // any BLOB size
	enum_,
	set,
	geometry,

	unknown,
	_not_computed,
};

}



#endif