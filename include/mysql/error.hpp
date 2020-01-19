#ifndef MYSQL_ASIO_ERROR_HPP
#define MYSQL_ASIO_ERROR_HPP

#include <boost/system/error_code.hpp>
#include <string>

namespace mysql
{

/// MySQL-specific error codes.
enum class Error : int
{
	// OK
	ok = 0,

	// Server returned errors
	#include "mysql/impl/server_error_enum.hpp"

	// Protocol errors
	incomplete_message = 0x10000,
	extra_bytes,
	sequence_number_mismatch,
	server_unsupported,
	protocol_value_error,
	unknown_auth_plugin
};

/// An alias for boost::system error codes.
using error_code = boost::system::error_code;

/// Additional information about error conditions
class error_info
{
	std::string msg_;
public:
	error_info(std::string&& err = {}): msg_(std::move(err)) {}
	const std::string& message() const noexcept { return msg_; }
	void set_message(std::string&& err) { msg_ = std::move(err); }
	void clear() noexcept { msg_.clear(); }
};
inline bool operator==(const error_info& lhs, const error_info& rhs) noexcept { return lhs.message() == rhs.message(); }
inline bool operator!=(const error_info& lhs, const error_info& rhs) noexcept { return !(lhs==rhs); }
inline std::ostream& operator<<(std::ostream& os, const error_info& v) { return os << v.message(); }

}

#include "mysql/impl/error.hpp"

#endif
