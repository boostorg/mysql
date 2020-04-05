#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUTH_AUTH_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUTH_AUTH_HPP_

#include "boost/mysql/detail/auth/mysql_native_password.hpp"
#include "boost/mysql/detail/auth/caching_sha2_password.hpp"
#include "boost/mysql/error.hpp"

namespace boost {
namespace mysql {
namespace detail {

class auth_response_calculator
{
	//std::array<std::uint8_t, mysql_native_password::response_length> auth_response_buffer_ {};
	//std::string_view res_;
	//bool use_buffer_ {};
	std::string response_;
	std::string plugin_name_;
public:
	error_code calculate(
		std::string_view plugin_name,
		std::string_view password,
		std::string_view challenge,
		bool allow_unknown_plugin,
		[[maybe_unused]] bool use_ssl
	)
	{
		// Find plugin_name in our list
		// If not found
		//   allow_unknown_plugin => set response to blank, return
		//   else => return error
		// Check for SSL requirements
		//
		plugin_name_ = plugin_name;

		// Blank password: we should just return an empty auth string
		if (password.empty())
		{
			response_ = "";
			return error_code();
		}

		if (plugin_name == mysql_native_password::plugin_name)
		{
			// Check challenge size
			if (challenge.size() != mysql_native_password::challenge_length)
			{
				return make_error_code(errc::protocol_value_error);
			}

			// Do the calculation
			std::array<std::uint8_t, mysql_native_password::response_length> buff;
			mysql_native_password::compute_auth_string(
				password,
				challenge.data(),
				buff.data()
			);
			response_.assign(reinterpret_cast<const char*>(buff.data()), buff.size());
			return error_code();
		}
		else if (plugin_name == "caching_sha2_password" ||
				 plugin_name == "sha256_password")
		{
			// TODO: use_ssl
			// Check challenge size
			if (challenge.size() != caching_sha2_password::challenge_length)
			{
				return make_error_code(errc::protocol_value_error);
			}

			// Do the calculation
			std::array<std::uint8_t, caching_sha2_password::response_length> buff;
			caching_sha2_password::compute_auth_string(
				password,
				challenge.data(),
				buff.data()
			);
			response_.assign(reinterpret_cast<const char*>(buff.data()), buff.size());
			return error_code();
		}
		else
		{
			response_ = "";
			return allow_unknown_plugin ? error_code() : make_error_code(errc::unknown_auth_plugin);
		}
	}

	std::string_view response() const noexcept
	{
		return response_;
	}
	const auto& get_plugin_name() const { return plugin_name_; }
};

}
}
}



#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUTH_AUTH_HPP_ */
