#ifndef BASIC_TYPES_H_
#define BASIC_TYPES_H_

#include <cstdint>
#include <string_view>

namespace mysql
{

using ReadIterator = const std::uint8_t*;
using WriteIterator = std::uint8_t*;

using int1 = std::uint8_t;
using int2 = std::uint16_t;
struct int3 { std::uint32_t value; };
using int4 = std::uint32_t;
struct int6 { std::uint64_t value; };
using int8 = std::uint64_t;
struct int_lenenc { std::uint64_t value; };
template <std::size_t size> using string_fixed = char[size];
struct string_null { std::string_view value; };
struct string_eof { std::string_view value; };
struct string_lenenc { std::string_view value; };

}



#endif /* BASIC_TYPES_H_ */
