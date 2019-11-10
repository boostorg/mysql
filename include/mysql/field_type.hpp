#ifndef INCLUDE_MYSQL_FIELD_TYPE_HPP_
#define INCLUDE_MYSQL_FIELD_TYPE_HPP_

#include <cstdint>

namespace mysql
{

enum class field_type : std::uint8_t
{
	decimal = 0x00, // DECIMAL
	tiny = 0x01, // TINYINT
	short_ = 0x02, // SMALLINT
	long_ = 0x03,
	float_ = 0x04,
	double_ = 0x05,
	null = 0x06,
	timestamp = 0x07,
	longlong = 0x08,
	int24 = 0x09, // MEDIUMINT
	date = 0x0a,
	time = 0x0b,
	datetime = 0x0c,
	year = 0x0d,
	varchar = 0x0f, // TODO: is this sent? what is sent for TEXT?
	bit = 0x10,
	newdecimal = 0xf6, // TODO: is this sent?
	enum_ = 0xf7,
	set = 0xf8, // TODO: investigate
	tiny_blob = 0xf9, // TODO: investigate
	medium_blob = 0xfa, // TODO: investigate
	long_blob = 0xfb, // TODO: investigate
	blob = 0xfc,
	var_string = 0xfd,
	string = 0xfe, // TODO: is this sent?
	geometry = 0xff // TODO: investigate
};


}



#endif /* INCLUDE_MYSQL_FIELD_TYPE_HPP_ */
