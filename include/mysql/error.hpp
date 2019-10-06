#ifndef INCLUDE_ERROR_HPP_
#define INCLUDE_ERROR_HPP_

#include <boost/system/error_code.hpp>

namespace mysql
{

enum class Error : int
{
	ok = 0,
	incomplete_message,
	sequence_number_mismatch
};

using error_code = boost::system::error_code;

}

#include "mysql/impl/error_impl.hpp"

#endif /* INCLUDE_ERROR_HPP_ */
