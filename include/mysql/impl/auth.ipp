#ifndef MYSQL_ASIO_IMPL_AUTH_IPP
#define MYSQL_ASIO_IMPL_AUTH_IPP

#include <openssl/sha.h>
#include <cstring>

// SHA1( password ) XOR SHA1( "20-bytes random data from server" <concat> SHA1( SHA1( password ) ) )
inline void mysql::detail::mysql_native_password::compute_auth_string(
	std::string_view password,
	const void* challenge,
	void* output
)
{
	// SHA1 (password)
	using sha1_buffer = unsigned char [SHA_DIGEST_LENGTH];
	sha1_buffer password_sha1;
	SHA1(reinterpret_cast<const unsigned char*>(password.data()), password.size(), password_sha1);

	// Add server challenge (salt)
	unsigned char salted_buffer [challenge_length + SHA_DIGEST_LENGTH];
	memcpy(salted_buffer, challenge, challenge_length);
	SHA1(password_sha1, sizeof(password_sha1), salted_buffer + 20);
	sha1_buffer salted_sha1;
	SHA1(salted_buffer, sizeof(salted_buffer), salted_sha1);

	// XOR
	static_assert(response_length == SHA_DIGEST_LENGTH);
	for (std::size_t i = 0; i < SHA_DIGEST_LENGTH; ++i)
	{
		static_cast<std::uint8_t*>(output)[i] = password_sha1[i] ^ salted_sha1[i];
	}
}





#endif