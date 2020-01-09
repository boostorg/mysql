#ifndef INCLUDE_AUTH_HPP_
#define INCLUDE_AUTH_HPP_

#include <cstdint>
#include <string_view>
#include <array>

namespace mysql
{
namespace detail
{

namespace mysql_native_password
{

constexpr const char* plugin_name = "mysql_native_password";
constexpr std::size_t challenge_length = 20;
constexpr std::size_t response_length = 20;

// challenge must point to challenge_length bytes of data
// output must point to response_length bytes of data
inline void compute_auth_string(std::string_view password, const void* challenge, void* output);

} // mysql_native_password



} // detail
} // mysql

#include "mysql/impl/auth.ipp"

#endif /* INCLUDE_AUTH_HPP_ */
