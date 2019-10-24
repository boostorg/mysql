#ifndef INCLUDE_IMPL_ERROR_IMPL_HPP_
#define INCLUDE_IMPL_ERROR_IMPL_HPP_

#include <boost/system/error_code.hpp>

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
	case Error::server_returned_error: return "The server returned an ERR_Packet";
	case Error::server_unsupported: return "The server does not implement the minimum features to be supported";
	case Error::protocol_value_error: return "A field in a message had an unexpected value";
	case Error::auth_error: return "Authentication failure";
	case Error::unknown_auth_plugin: return "The user employs an authentication plugin unknown to the client";
	default: return "<unknown error>";
	}
}

class MysqlErrorCategory : public boost::system::error_category
{
public:
	const char* name() const noexcept final override { return "mysql"; }
	std::string message(int ev) const final override
	{
		return error_to_string(static_cast<Error>(ev));
	}
};

inline const boost::system::error_category& get_mysql_error_category()
{
	static MysqlErrorCategory res;
	return res;
}


inline boost::system::error_code make_error_code(Error error)
{
	return boost::system::error_code(
		static_cast<int>(error), get_mysql_error_category()
	);
}

}
}




#endif /* INCLUDE_IMPL_ERROR_IMPL_HPP_ */
