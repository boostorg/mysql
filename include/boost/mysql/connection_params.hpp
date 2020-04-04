#ifndef INCLUDE_BOOST_MYSQL_CONNECTION_PARAMS_HPP_
#define INCLUDE_BOOST_MYSQL_CONNECTION_PARAMS_HPP_

#include <string_view>
#include "boost/mysql/collation.hpp"

namespace boost {
namespace mysql {

enum class ssl_mode
{
	disable,
	require
};

class ssl_options
{
	ssl_mode mode_;
public:
	explicit ssl_options(ssl_mode mode=ssl_mode::require) noexcept:
		mode_(mode) {}
	ssl_mode mode() const noexcept { return mode_; }
};


/**
 * \brief Parameters defining how to authenticate to a MySQL server.
 */
class connection_params
{
	std::string_view username_;
	std::string_view password_;
	std::string_view database_;
	collation connection_collation_;
	ssl_options ssl_;
public:
	/// Initializing constructor
	connection_params(
		std::string_view username,  ///< Username to authenticate as
		std::string_view password,  ///< Password for that username, possibly empty.
		std::string_view db = "",   ///< Database to use, or empty string for no database.
		collation connection_col = collation::utf8_general_ci, ///< The default character set and collation for the connection.
		const ssl_options& opts = ssl_options()
	) :
		username_(username),
		password_(password),
		database_(db),
		connection_collation_(connection_col),
		ssl_(opts)
	{
	}

	/// Retrieves the username.
	std::string_view username() const noexcept { return username_; }

	/// Sets the username.
	void set_username(std::string_view value) noexcept { username_ = value; }

	/// Retrieves the password.
	std::string_view password() const noexcept { return password_; }

	/// Sets the password
	void set_password(std::string_view value) noexcept { password_ = value; }

	/// Retrieves the database.
	std::string_view database() const noexcept { return database_; }

	/// Sets the database
	void set_database(std::string_view value) noexcept { database_ = value; }

	/// Retrieves the connection collation.
	collation connection_collation() const noexcept { return connection_collation_; }

	/// Sets the connection collation
	void set_connection_collation(collation value) noexcept { connection_collation_ = value; }

	/// Retrieves SSL options
	const ssl_options& ssl() const noexcept { return ssl_; }

	/// Sets SSL options
	void set_ssl(const ssl_options& value) noexcept { ssl_ = value; }
};

}
}



#endif /* INCLUDE_BOOST_MYSQL_CONNECTION_PARAMS_HPP_ */
