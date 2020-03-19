#ifndef TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP_
#define TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP_

#include "boost/mysql/connection.hpp"
#include "test_common.hpp"
#include <gtest/gtest.h>
#include <forward_list>

namespace boost {
namespace mysql {
namespace test {

struct no_result {};

template <typename T>
struct network_result
{
	error_code err;
	error_info info;
	T value;

	void validate_no_error() const
	{
		ASSERT_EQ(err, error_code());
		EXPECT_EQ(info, error_info());
	}

	void validate_error(
		error_code expected_errc,
		const std::vector<std::string>& expected_msg
	) const
	{
		EXPECT_EQ(err, expected_errc);
		validate_string_contains(info.message(), expected_msg);
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

class network_functions
{
public:
	virtual ~network_functions() = default;
	virtual const char* name() const = 0;
	virtual network_result<no_result> handshake(tcp_connection&, const connection_params&) = 0;
	virtual network_result<tcp_resultset> query(tcp_connection&, std::string_view query) = 0;
	virtual network_result<tcp_prepared_statement> prepare_statement(
			tcp_connection&, std::string_view statement) = 0;
	virtual network_result<tcp_resultset> execute_statement(
			tcp_prepared_statement&, value_list_it params_first, value_list_it params_last) = 0;
	virtual network_result<tcp_resultset> execute_statement(
			tcp_prepared_statement&, const std::vector<value>&) = 0;
	virtual network_result<no_result> close_statement(tcp_prepared_statement&) = 0;
	virtual network_result<const row*> fetch_one(tcp_resultset&) = 0;
	virtual network_result<std::vector<owning_row>> fetch_many(tcp_resultset&, std::size_t count) = 0;
	virtual network_result<std::vector<owning_row>> fetch_all(tcp_resultset&) = 0;
};

extern network_functions* sync_errc_network_functions;
extern network_functions* sync_exc_network_functions;
extern network_functions* async_network_functions;

inline network_functions* all_network_functions [] = {
	sync_errc_network_functions,
	sync_exc_network_functions,
	async_network_functions
};

#define MYSQL_NETWORK_TEST_SUITE(TestSuiteName) \
	INSTANTIATE_TEST_SUITE_P(Default, TestSuiteName, testing::ValuesIn( \
		all_network_functions \
	), [](const auto& param_info) { return param_info.param->name(); })


} // test
} // mysql
} // boost



#endif /* TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP_ */
