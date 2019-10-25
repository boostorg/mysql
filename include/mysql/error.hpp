#ifndef INCLUDE_ERROR_HPP_
#define INCLUDE_ERROR_HPP_

#include <boost/system/error_code.hpp>

namespace mysql
{

enum class Error : int
{
	// OK
	ok = 0,

	// Server returned errors
	#include "mysql/impl/server_error_codes.hpp"

	// Protocol errors
	incomplete_message = 0x10000,
	extra_bytes,
	sequence_number_mismatch,
	server_unsupported,
	protocol_value_error,
	unknown_auth_plugin
};

using error_code = boost::system::error_code;

}

#include "mysql/impl/error_impl.hpp"

#endif /* INCLUDE_ERROR_HPP_ */
