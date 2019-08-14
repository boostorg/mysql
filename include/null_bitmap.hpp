#ifndef INCLUDE_NULL_BITMAP_HPP_
#define INCLUDE_NULL_BITMAP_HPP_

#include <cstddef>

namespace mysql
{

template <std::size_t offset>
class NullBitmapTraits
{
	std::size_t num_fields_;
public:
	constexpr NullBitmapTraits(std::size_t num_fields): num_fields_ {num_fields} {};
	std::size_t byte_count() const { return (num_fields_ + 7 + offset) / 8; }
	std::size_t byte_pos(std::size_t field_pos) const { return (field_pos + offset) / 8; }
	std::size_t bit_pos(std::size_t field_pos) const { return (field_pos + offset) % 8; }
	bool is_null(ReadIterator null_bitmap_begin, std::size_t field_pos) const
	{
		return null_bitmap_begin[byte_pos(field_pos)] & (1 << bit_pos(field_pos));
	}
};

using StmtExecuteNullBitmapTraits = NullBitmapTraits<0>;
using ResultsetRowNullBitmapTraits = NullBitmapTraits<2>;


}



#endif /* INCLUDE_NULL_BITMAP_HPP_ */
