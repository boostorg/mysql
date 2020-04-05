#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUTH_IMPL_CACHING_SHA2_PASSWORD_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUTH_IMPL_CACHING_SHA2_PASSWORD_HPP_

#include <openssl/sha.h>
#include <cstring>

inline void boost::mysql::detail::caching_sha2_password::compute_auth_string(
	std::string_view password,
	const void* challenge,
	void* output
)
{
	static_assert(response_length == SHA256_DIGEST_LENGTH);

	// SHA(SHA(password_sha) concat challenge) XOR password_sha
	// hash1 = SHA(pass)
	using sha_buffer = std::uint8_t [response_length];
	sha_buffer password_sha;
	SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), password_sha);

	// SHA(password_sha) concat challenge = buffer
	std::uint8_t buffer [response_length + challenge_length];
	SHA256(password_sha, response_length, buffer);
	std::memcpy(buffer + response_length, challenge, challenge_length);

	// SHA(SHA(password_sha) concat challenge) = SHA(buffer) = salted_password
	sha_buffer salted_password;
	SHA256(buffer, sizeof(buffer), salted_password);

	// salted_password XOR password_sha
	for (unsigned i = 0; i < response_length; ++i)
	{
		static_cast<std::uint8_t*>(output)[i] = salted_password[i] ^ password_sha[i];
	}
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUTH_IMPL_CACHING_SHA2_PASSWORD_HPP_ */
