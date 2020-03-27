#ifndef INCLUDE_BOOST_MYSQL_ASYNC_HANDLER_ARG_HPP_
#define INCLUDE_BOOST_MYSQL_ASYNC_HANDLER_ARG_HPP_

#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {

template <typename T>
class async_handler_arg
{
	error_info err_;
	T value_;
public:
	constexpr async_handler_arg() noexcept = default;

	constexpr async_handler_arg(error_info&& info):
		err_(std::move(info)) {}

	template <typename... Args>
	constexpr async_handler_arg(Args&&... args):
		value_(std::forward<Args>(args)...) {}

	const error_info& error() const noexcept { return err_; }
	const T& get() const noexcept { return value_; }
	T& get() noexcept { return value_; }
};


} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_ASYNC_HANDLER_ARG_HPP_ */
