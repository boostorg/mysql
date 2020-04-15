           #ifndef TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP_
#define TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP_

#include "boost/mysql/connection.hpp"
#include "test_common.hpp"
#include <gtest/gtest.h>
#include <forward_list>
#include <optional>

namespace boost {
namespace mysql {
namespace test {

struct no_result {};

template <typename T>
struct network_result
{
	error_code err;
	std::optional<error_info> info; // some async initiators (futures) don't support this
	T value;

	network_result() = default;

	network_result(error_code ec, error_info info, T&& value = {}):
		err(ec), info(std::move(info)), value(std::move(value)) {}

	network_result(error_code ec, T&& value = {}):
		err(ec), value(std::move(value)) {}

	void validate_no_error() const
	{
		ASSERT_EQ(err, error_code());
		if (info)
		{
			EXPECT_EQ(*info, error_info());
		}
	}

	void validate_error(
		error_code expected_errc,
		const std::vector<std::string>& expected_msg
	) const
	{
		EXPECT_EQ(err, expected_errc);
		if (info)
		{
			validate_string_contains(info->message(), expected_msg);
		}
	}

	void validate_error(
		errc expected_errc,
		const std::vector<std::string>& expected_msg
	) const
	{
		validate_error(detail::make_error_code(expected_errc), expected_msg);
	}
};

using value_list_it = std::forward_list<value>::const_iterator;

template <typename Stream>
class network_functions
{
public:
	using connection_type = connection<Stream>;
	using prepared_statement_type = prepared_statement<Stream>;
	using resultset_type = resultset<Stream>;

	virtual ~network_functions() = default;
	virtual const char* name() const = 0;
	virtual network_result<no_result> handshake(connection_type&, const connection_params&) = 0;
	virtual network_result<resultset_type> query(connection_type&, std::string_view query) = 0;
	virtual network_result<prepared_statement_type> prepare_statement(
			connection_type&, std::string_view statement) = 0;
	virtual network_result<resultset_type> execute_statement(
			prepared_statement_type&, value_list_it params_first, value_list_it params_last) = 0;
	virtual network_result<resultset_type> execute_statement(
			prepared_statement_type&, const std::vector<value>&) = 0;
	virtual network_result<no_result> close_statement(prepared_statement_type&) = 0;
	virtual network_result<const row*> fetch_one(resultset_type&) = 0;
	virtual network_result<std::vector<owning_row>> fetch_many(resultset_type&, std::size_t count) = 0;
	virtual network_result<std::vector<owning_row>> fetch_all(resultset_type&) = 0;
};

template <typename Stream>
using network_function_array = std::array<network_functions<Stream>*, 7>;

template <typename Stream>
network_function_array<Stream> make_all_network_functions();

} // test
} // mysql
} // boost



#endif /* TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP_ */
