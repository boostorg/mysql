#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUTH_CACHING_SHA2_PASSWORD_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUTH_CACHING_SHA2_PASSWORD_HPP_

#include <cstddef>
#include <string_view>

namespace boost {
namespace mysql {
namespace detail {
namespace caching_sha2_password {

constexpr const char* plugin_name = "caching_sha2_password";
constexpr std::size_t challenge_length = 20;
constexpr std::size_t response_length = 32;

// challenge must point to challenge_length bytes of data
// output must point to response_length bytes of data
inline void compute_auth_string(std::string_view password, const void* challenge, void* output);

}
}
}
}

#include "boost/mysql/detail/auth/impl/caching_sha2_password.hpp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUTH_CACHING_SHA2_PASSWORD_HPP_ */
