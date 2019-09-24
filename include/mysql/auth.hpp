#ifndef INCLUDE_AUTH_HPP_
#define INCLUDE_AUTH_HPP_

#include <cstdint>
#include <string_view>

namespace mysql
{

namespace mysql_native_password
{

constexpr std::size_t challenge_length = 20;
constexpr std::size_t response_length = 20;
using response_buffer = std::uint8_t [response_length];

// challenge must point to challenge_length bytes of data
void compute_auth_string(std::string_view password, const void* challenge, response_buffer& output);

}




}



#endif /* INCLUDE_AUTH_HPP_ */
