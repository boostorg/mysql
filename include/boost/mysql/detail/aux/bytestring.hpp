#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUX_BYTESTRING_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUX_BYTESTRING_HPP_

#include <vector>

namespace boost {
namespace mysql {
namespace detail {

template <typename Allocator>
using basic_bytestring = std::vector<std::uint8_t, Allocator>;

using bytestring = std::vector<std::uint8_t>;

}
}
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUX_BYTESTRING_HPP_ */
