#ifndef INCLUDE_ERROR_HPP_
#define INCLUDE_ERROR_HPP_

#include <boost/system/error_code.hpp>

namespace mysql
{

enum class Error : int
{
	ok = 0,
	incomplete_message,
	extra_bytes,
	sequence_number_mismatch,
	server_returned_error,
	server_unsupported,
	protocol_value_error
};

using error_code = boost::system::error_code;

}

#include "mysql/impl/error_impl.hpp"

#endif /* INCLUDE_ERROR_HPP_ */
