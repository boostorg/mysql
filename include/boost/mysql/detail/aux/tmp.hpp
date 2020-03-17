#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUX_TMP_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUX_TMP_HPP_

#include <type_traits>

namespace mysql
{
namespace detail
{

template <typename T, typename Head, typename... Tail>
struct is_one_of
{
	static constexpr bool value = std::is_same_v<T, Head> || is_one_of<T, Tail...>::value;
};

template <typename T, typename Head>
struct is_one_of<T, Head>
{
	static constexpr bool value = std::is_same_v<T, Head>;
};


template <typename T, typename... Types>
constexpr bool is_one_of_v = is_one_of<T, Types...>::value;

}
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUX_TMP_HPP_ */
