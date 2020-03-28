#ifndef INCLUDE_BOOST_MYSQL_ASYNC_HANDLER_ARG_HPP_
#define INCLUDE_BOOST_MYSQL_ASYNC_HANDLER_ARG_HPP_

#include "boost/mysql/error.hpp"
#include <type_traits>

namespace boost {
namespace mysql {

/**
 * \brief An error_info plus another type.
 * \details This type is intended to be used as second argument in
 * an asynchronous handler signature, in cases where both an error_info
 * and another type must be transmitted.
 *
 * In order for Boost.Asio universal asynchronous primitives to work, all
 * handler signatures should have two arguments, at most, and the first one
 * should be an error_code. Concretely, stackful coroutines (boost::asio::yield_context)
 * do NOT support handlers with more than two arguments. However, many handlers
 * in this library need to transmit three arguments: an error_code, an error_info
 * and another type (e.g. a resultset, a prepared_statement...). This class is
 * intended to be used as second argument in these handlers. It is similar to
 * a std::pair, but accessor names make more sense.
 *
 * This class is NOT intended to be created by the user - the library
 * will pass it to your asynchronous handlers.
 *
 * async_handler_arg supports default cnstruction, copy and move operations as
 * long as T supports them.
 */
template <typename T>
class async_handler_arg
{
	error_info err_;
	T value_;
public:
	/// The type T.
	using value_type = T;

	/// Default constructor.
	constexpr async_handler_arg() = default;

	// Private, do not use
	constexpr async_handler_arg(error_info&& info):
		err_(std::move(info)) {}

	// Private, do not use
	template <typename... Args>
	constexpr async_handler_arg(Args&&... args):
		value_(std::forward<Args>(args)...) {}

	/// Retrieves the stored error_info.
	const error_info& error() const noexcept { return err_; }

	/// Retrieves the stored value (const version).
	const T& get() const & noexcept { return value_; }

	/// Retrieves the stored value (non-const version).
	T& get() & noexcept { return value_; }

	/// Retrieves the stored value (rvalue version).
	T&& get() && noexcept { return std::move(value_); }
};


} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_ASYNC_HANDLER_ARG_HPP_ */
