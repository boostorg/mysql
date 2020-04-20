//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_CHECK_COMPLETION_TOKEN_HPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_CHECK_COMPLETION_TOKEN_HPP

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
	if constexpr (std::is_same_v<HandlerArg, void>)
	{
		return std::is_invocable_v<HandlerType, error_code>;
	}
	else
	{
		return std::is_invocable_v<HandlerType, error_code, HandlerArg>;
	}
}

template <typename CompletionToken, typename HandlerSignature>
constexpr void check_completion_token()
{
	using handler_type = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using handler_arg = typename detail::get_handler_arg<HandlerSignature>::type;
	static_assert(
		is_handler_signature_ok<handler_type, handler_arg>(),
		"Invalid CompletionToken type. Check that CompletionToken fullfills the CompletionToken "
			"requirements or that the callback signature you passed in is correct"
	);
}

} // detail
} // mysql
} // boost



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUXILIAR_CHECK_COMPLETION_TOKEN_HPP_ */
