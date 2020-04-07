#ifndef MYSQL_ASIO_ERROR_HPP
#define MYSQL_ASIO_ERROR_HPP

#include <boost/system/error_code.hpp>
#include <string>

/**
 * \defgroup error Error handling
 * \brief Classes and functions used in error handling.
 */

namespace boost {
namespace mysql {

/**
 * \ingroup error
 * \brief MySQL-specific error codes.
 * \details Some error codes are defined by the client library, and others
 * are returned from the server. For the latter, the numeric value and
 * string descriptions match the ones described in the MySQL documentation.
 * Only the first ones are documented here. For the latter, see
 * https://dev.mysql.com/doc/refman/8.0/en/server-error-reference.html
 */
enum class errc : int
{
	ok = 0, ///< No error.

	// Server returned errors
	#include "boost/mysql/impl/server_error_enum.hpp"

	// Protocol errors
	incomplete_message = 0x10000, ///< An incomplete message was received from the server.
	extra_bytes, ///< Unexpected extra bytes at the end of a message were received.
	sequence_number_mismatch, ///< A sequence number mismatched happened.
	server_unsupported, ///< The server does not support the minimum required capabilities to establish the connection.
	protocol_value_error, ///< An unexpected value was found in a server-received message.
	unknown_auth_plugin, ///< The user employs an authentication plugin not known to this library.
	auth_plugin_requires_ssl, ///< The authentication plugin requires the connection to use SSL.
	wrong_num_params ///< The number of parameters passed to the prepared statement does not match the number of actual parameters.
};

/**
 * \ingroup error
 * \brief An alias for boost::system error codes.
 */
using error_code = boost::system::error_code;

/**
 * \ingroup error
 * \brief Additional information about error conditions
 * \details Contains an error message describing what happened. Not all error
 * conditions are able to generate this extended information - those that
 * can't have an empty error message.
 */
class error_info
{
	std::string msg_;
public:
	/// Default constructor.
	error_info() = default;

	/// Initialization constructor.
	error_info(std::string&& err) noexcept: msg_(std::move(err)) {}

	/// Gets the error message.
	const std::string& message() const noexcept { return msg_; }

	/// Sets the error message.
	void set_message(std::string&& err) { msg_ = std::move(err); }

	/// Restores the object to its initial state.
	void clear() noexcept { msg_.clear(); }
};

/**
 * \relates error_info
 * \brief Compare two error_info objects.
 */
inline bool operator==(const error_info& lhs, const error_info& rhs) noexcept { return lhs.message() == rhs.message(); }

/**
 * \relates error_info
 * \brief Compare two error_info objects.
 */
inline bool operator!=(const error_info& lhs, const error_info& rhs) noexcept { return !(lhs==rhs); }

/**
 * \relates error_info
 * \brief Streams an error_info.
 */
inline std::ostream& operator<<(std::ostream& os, const error_info& v) { return os << v.message(); }

} // mysql
} // boost

#include "boost/mysql/impl/error.hpp"

#endif
