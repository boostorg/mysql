#ifndef MYSQL_ASIO_CONNECTION_HPP
#define MYSQL_ASIO_CONNECTION_HPP

#include "mysql/impl/channel.hpp"
#include "mysql/impl/network_algorithms/handshake.hpp"
#include "mysql/impl/basic_types.hpp"
#include "mysql/error.hpp"
#include "mysql/resultset.hpp"
#include "mysql/prepared_statement.hpp"
#include <boost/asio/ip/tcp.hpp>

namespace mysql
{

/**
 * \brief Parameters defining how to authenticate to a MySQL server.
 */
struct connection_params
{
	std::string_view username; ///< Username to authenticate as.
	std::string_view password; ///< Password for that username, possibly empty.
	std::string_view database; ///< Database to use, or empty string for no database.
	collation connection_collation; ///< The default character set and collation for the connection.

	/// Initializing constructor
	connection_params(
		std::string_view username,
		std::string_view password,
		std::string_view db = "",
		collation connection_col = collation::utf8_general_ci
	) :
		username(username),
		password(password),
		database(db),
		connection_collation(connection_col)
	{
	}
};

/**
 * \brief A connection to a MySQL server.
 * \details
 * This is the basic object to instantiate to use the MySQL-Asio library.
 *
 * Connections allow you to interact with the database server (e.g. you
 * can issue queries using connection::query).
 *
 * Before being able to use a connection, you must connect it. This encompasses two steps:
 *   - Stream connection: you should make sure the Stream gets connected to
 *     whatever endpoint the MySQL server lives on. You can access the underlying
 *     Stream object by calling connection::next_layer(). For a TCP socket, this is equivalent
 *     to connecting the socket.
 *   - MySQL handshake: authenticates the connection to the MySQL server. You can do that
 *     by calling connection::handshake() or connection::async_handshake().
 *
 * Because of how the MySQL protocol works, you must fully perform an operation before
 * starting the next one. For queries, you must wait for the query response and
 * **read the entire resultset** before starting a new query. For prepared statements,
 * once executed, you must wait for the response and read the entire resultset.
 * Otherwise, results are undefined.
 */
template <
	typename Stream ///< The underlying stream to use; must satisfy Boost.Asio's SyncStream and AsyncStream concepts.
>
class connection
{
	using channel_type = detail::channel<Stream>;

	Stream next_level_;
	channel_type channel_;
public:
	/**
	 * \brief Initializing constructor.
	 * \details Creates a Stream object by forwarding any passed in arguments to its constructor.
	 */
	template <typename... Args>
	connection(Args&&... args) :
		next_level_(std::forward<Args>(args)...),
		channel_(next_level_)
	{
	}

	/// Retrieves the underlying Stream object.
	Stream& next_level() { return next_level_; }

	/// Retrieves the underlying Stream object.
	const Stream& next_level() const { return next_level_; }

	/// Performs the MySQL-level handshake (synchronous with error code version).
	void handshake(const connection_params& params, error_code& ec, error_info& info);

	/// Performs the MySQL-level handshake (synchronous with exceptions version).
	void handshake(const connection_params& params);

	/**
	 * \brief Performs the MySQL-level handshake (asynchronous version).
	 * \details The strings pointed to by params should be kept alive by the caller
	 * until the operation completes, as no copy is made by the library.
	 */
	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, error_info))
	async_handshake(const connection_params& params, CompletionToken&& token);

	/**
	 * \brief Executes a SQL text query (sync with error code version).
	 * \detail Does not perform the actual retrieval of the data; use the various
	 * fetch functions within resultset to achieve that.
	 *
	 * Note that query_string may contain any valid SQL, not just SELECT statements.
	 * If your query does not return any data, then the resultset will be empty
	 * (\see resultset for more details).
	 *
	 * \warning After query() has returned, you should read the entire resultset
	 * before calling any function that involves communication with the server over this
	 * connection (like connection::query, connection::prepare_statement or
	 * prepared_statement::execute). Otherwise, the results are undefined.
	 */
	resultset<Stream> query(std::string_view query_string, error_code&, error_info&);

	/// Executes a SQL text query (sync with exceptions version).
	resultset<Stream> query(std::string_view query_string);

	/// Executes a SQL text query (async version).
	template <typename CompletionToken>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, error_info, resultset<Stream>))
	async_query(std::string_view query_string, CompletionToken&& token);

	/**
	 * \brief Prepares a statement in the server (sync with error code version).
	 * \details Instructs the server to create a prepared statement. The passed
	 * in statement should be a SQL statement with question marks (?) as placeholders
	 * for the statement parameters. See the MySQL documentation on prepared statements
	 * for more info.
	 *
	 * Prepared statements are only valid while the connection object on which
	 * prepare_statement was called is alive and open. See prepared_statement docs
	 * for more info.
	 */
	prepared_statement<Stream> prepare_statement(std::string_view statement, error_code&, error_info&);

	/// Prepares a statement (sync with exceptions version).
	prepared_statement<Stream> prepare_statement(std::string_view statement);

	/// Prepares a statement (async version).
	template <typename CompletionToken>
	auto async_prepare_statement(std::string_view statement, CompletionToken&& token);
};

/// A connection to MySQL over TCP.
using tcp_connection = connection<boost::asio::ip::tcp::socket>;

// TODO: UNIX socket connection

/// The default TCP port for the MySQL protocol.
constexpr unsigned short default_port = 3306;

}

#include "mysql/impl/connection.ipp"

#endif
