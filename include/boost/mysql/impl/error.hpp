#ifndef MYSQL_ASIO_IMPL_ERROR_HPP
#define MYSQL_ASIO_IMPL_ERROR_HPP

#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

namespace boost {
namespace system {

template <>
struct is_error_code_enum<mysql::errc>
{
	static constexpr bool value = true;
};

} // system

namespace mysql {
namespace detail {

inline const char* error_to_string(errc error) noexcept
{
	switch (error)
	{
	case errc::ok: return "no error";
	case errc::incomplete_message: return "The message read was incomplete (not enough bytes to fully decode it)";
	case errc::extra_bytes: return "Extra bytes at the end of the message";
	case errc::sequence_number_mismatch: return "Mismatched sequence numbers";
	case errc::server_unsupported: return "The server does not implement the minimum features to be supported";
	case errc::protocol_value_error: return "A field in a message had an unexpected value";
	case errc::unknown_auth_plugin: return "The user employs an authentication plugin unknown to the client";
	case errc::wrong_num_params: return "The provided parameter count does not match the prepared statement parameter count";

	#include "boost/mysql/impl/server_error_descriptions.hpp"

	default: return "<unknown error>";
	}
}

class mysql_error_category_t : public boost::system::error_category
{
public:
	const char* name() const noexcept final override { return "mysql"; }
	std::string message(int ev) const final override
	{
		return error_to_string(static_cast<errc>(ev));
	}
};
inline mysql_error_category_t mysql_error_category;

inline boost::system::error_code make_error_code(errc error)
{
	return boost::system::error_code(static_cast<int>(error), mysql_error_category);
}

inline void check_error_code(const error_code& code, const error_info& info)
{
	if (code)
	{
		throw boost::system::system_error(code, info.message());
	}
}

} // detail
} // mysql
} // boost


#endif
