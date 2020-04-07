#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUTH_CACHING_SHA2_PASSWORD_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUTH_CACHING_SHA2_PASSWORD_HPP_

#include <cstddef>
#include <string_view>
#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {
namespace detail {
namespace caching_sha2_password {

// Authorization for this plugin may be cleartext password or challenge/response.
// The server has a cache that uses when employing challenge/response. When
// the server sends a challenge of challenge_length bytes, we should send
// the password hashed with the challenge. The server may send a challenge
// equals to perform_full_auth, meaning it could not use the cache to
// complete the auth. In this case, we should just send the cleartext password.
// Doing the latter requires a SSL connection. It is possible to perform full
// auth without an SSL connection, but that requires the server public key,
// and we do not implement that.
inline error_code compute_response(
	std::string_view password,
	std::string_view challenge,
	bool use_ssl,
	std::string& output
);

} // caching_sha2_password
} // detail
} // mysql
} // boost

#include "boost/mysql/detail/auth/impl/caching_sha2_password.ipp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUTH_CACHING_SHA2_PASSWORD_HPP_ */
