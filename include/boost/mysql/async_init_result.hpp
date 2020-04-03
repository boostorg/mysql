#ifndef INCLUDE_BOOST_MYSQL_ASYNC_INIT_RESULT_HPP_
#define INCLUDE_BOOST_MYSQL_ASYNC_INIT_RESULT_HPP_

#include <type_traits>
#include <boost/asio/async_result.hpp>
#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {

namespace detail {

template <typename HandlerSignature>
struct get_handler_arg;

template <typename T>
struct get_handler_arg<void(error_code, T)>
{
	using type = T;
};

template <>
struct get_handler_arg<void(error_code)>
{
	using type = void;
};

template <typename HandlerType, typename HandlerArg>
constexpr bool is_handler_signature_ok()
{
	if (std::is_same_v<HandlerArg, void>)
	{
		return std::is_invocable_v<HandlerType, error_code>;
	}
	else
	{
		return std::is_invocable_v<HandlerType, error_code, HandlerArg>;
	}
}

} // detail

/**
 * \brief Type trait to get the result of an initiating function.
 * \details Has a type member equal to
 * BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, HandlerSignature),
 * and performs requirements checking on CompletionToken.
 */
template <typename CompletionToken, typename HandlerSignature>
class async_init_result
{
	using handler_type = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using handler_arg = typename detail::get_handler_arg<HandlerSignature>::type;
	static_assert(
		detail::is_handler_signature_ok<handler_type, handler_arg>(),
		"Invalid CompletionToken type. Check that CompletionToken fullfills the CompletionToken "
			"requirements or that the callback signature you passed in is correct"
	);
public:
	/// The result of the metafunction.
	using type = BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, HandlerSignature);
};

/// Type alias for async_init_result.
template <typename CompletionToken, typename HandlerArg>
using async_init_result_t = typename async_init_result<CompletionToken, HandlerArg>::type;

}
}



#endif /* INCLUDE_BOOST_MYSQL_ASYNC_INIT_RESULT_HPP_ */
