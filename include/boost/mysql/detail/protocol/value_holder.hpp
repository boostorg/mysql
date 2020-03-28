#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_VALUE_HOLDER_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_VALUE_HOLDER_HPP_

#include <utility>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

template <typename T>
struct value_holder
{
	using value_type = T;
	static_assert(std::is_nothrow_default_constructible_v<T>);

	value_type value;

	value_holder() noexcept: value{} {};

	template <typename U>
	explicit constexpr value_holder(U&& u)
		noexcept(std::is_nothrow_constructible_v<T, decltype(u)>):
		value(std::forward<U>(u)) {}

	constexpr bool operator==(const value_holder<T>& rhs) const { return value == rhs.value; }
	constexpr bool operator!=(const value_holder<T>& rhs) const { return value != rhs.value; }
};

// Operations on value holders
struct get_value_type_helper
{
    struct no_value_type {};

    template <typename T>
    static constexpr typename T::value_type get(typename T::value_type*);

    template <typename T>
    static constexpr no_value_type get(...);
};

template <typename T>
struct get_value_type
{
    using type = decltype(get_value_type_helper().get<T>(nullptr));
    using no_value_type = get_value_type_helper::no_value_type;
};

template <typename T>
using get_value_type_t = typename get_value_type<T>::type;

}
}
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_VALUE_HOLDER_HPP_ */
