#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUTH_AUTH_CALCULATOR_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUTH_AUTH_CALCULATOR_HPP_

#include "boost/mysql/error.hpp"
#include <array>
#include <string>
#include <string_view>

namespace boost {
namespace mysql {
namespace detail {

struct authentication_plugin
{
	using calculator_signature = error_code (*)(
		std::string_view password,
		std::string_view challenge,
		bool use_ssl,
		std::string& output
	);

	std::string_view name;
	calculator_signature calculator;
};

class auth_calculator
{
	const authentication_plugin* plugin_ {nullptr};
	std::string response_;

	inline static const authentication_plugin* find_plugin(std::string_view name);
public:
	inline error_code calculate(
		std::string_view plugin_name,
		std::string_view password,
		std::string_view challenge,
		bool use_ssl
	);
	std::string_view response() const noexcept { return response_; }
	std::string_view plugin_name() const noexcept
	{
		assert(plugin_);
		return plugin_->name;
	}
};

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/auth/impl/auth_calculator.ipp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUTH_AUTH_CALCULATOR_HPP_ */
