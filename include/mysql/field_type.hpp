
#ifndef INCLUDE_MYSQL_FIELD_TYPE_HPP_
#define INCLUDE_MYSQL_FIELD_TYPE_HPP_

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



#endif /* INCLUDE_MYSQL_FIELD_TYPE_HPP_ */
