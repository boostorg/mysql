#ifndef MYSQL_ASIO_IMPL_ERROR_HPP
#define MYSQL_ASIO_IMPL_ERROR_HPP

#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

namespace boost
{
namespace system
{

template <>
struct is_error_code_enum<mysql::Error>
{
	static constexpr bool value = true;
};

} // system
} // boost


namespace mysql
{
namespace detail
{

inline const char* error_to_string(Error error) noexcept
{
	switch (error)
	{
	case Error::ok: return "no error";
	case Error::incomplete_message: return "The message read was incomplete (not enough bytes to fully decode it)";
	case Error::extra_bytes: return "Extra bytes at the end of the message";
	case Error::sequence_number_mismatch: return "Mismatched sequence numbers";
	case Error::server_unsupported: return "The server does not implement the minimum features to be supported";
	case Error::protocol_value_error: return "A field in a message had an unexpected value";
	case Error::unknown_auth_plugin: return "The user employs an authentication plugin unknown to the client";
	case Error::wrong_num_params: return "The provided parameter count does not match the prepared statement parameter count";

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
		return error_to_string(static_cast<Error>(ev));
	}
};
inline mysql_error_category_t mysql_error_category;

inline boost::system::error_code make_error_code(Error error)
{
	return boost::system::error_code(static_cast<int>(error), mysql_error_category);
}

inline void check_error_code(const error_code& errc, const error_info& info)
{
	if (errc)
	{
		throw boost::system::system_error(errc, info.message());
	}
}

}
}




#endif
