#ifndef MYSQL_ASIO_IMPL_AUTH_HPP
#define MYSQL_ASIO_IMPL_AUTH_HPP

#include <cstdint>
#include <string_view>
#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {
namespace detail {
namespace mysql_native_password {

// Authorization for this plugin is always challenge (nonce) -> response
// (hashed password).
inline error_code compute_response(
	std::string_view password,
	std::string_view challenge,
	bool use_ssl,
	std::string& output
);


} // mysql_native_password
} // detail
} // mysql
} // boost

#include "boost/mysql/detail/auth/impl/mysql_native_password.ipp"

#endif
