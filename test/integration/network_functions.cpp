#include "network_functions.hpp"
#include <future>

using namespace mysql::test;
using mysql::tcp_prepared_statement;
using mysql::tcp_resultset;
using mysql::tcp_connection;
using mysql::error_info;
using mysql::error_code;
using mysql::detail::make_error_code;
using mysql::Error;

namespace
{

template <typename Callable>
auto sync_errc_impl(Callable&& cb) {
	using R = decltype(cb(std::declval<error_code&>(), std::declval<error_info&>()));
	network_result<R> res;
	res.err = make_error_code(Error::no);
	res.info.set_message("Error info not cleared correctly");
	res.value = cb(res.err, res.info);
	return res;
}

class sync_errc : public network_functions
{
public:
	network_result<tcp_prepared_statement> prepare_statement(
		tcp_connection& conn,
		std::string_view statement
	) override
	{
		return sync_errc_impl([&conn, statement](error_code& err, error_info& info) {
			return conn.prepare_statement(statement, err, info);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		value_list_it params_first,
		value_list_it params_last
	) override
	{
		return sync_errc_impl([=, &stmt](error_code& err, error_info& info) {
			return stmt.execute(params_first, params_last, err, info);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		const std::vector<mysql::value>& values
	) override
	{
		return sync_errc_impl([&stmt, &values](error_code& err, error_info& info) {
			return stmt.execute(values, err, info);
		});
	}
};
sync_errc sync_errc_obj;

template <typename Callable>
auto sync_exc_impl(Callable&& cb) {
	using R = decltype(cb());
	network_result<R> res;
	try
	{
		res.value = cb();
	}
	catch (const boost::system::system_error& err)
	{
		res.err = err.code();
		res.info.set_message(err.what());
	}
	return res;
}

class sync_exc : public network_functions
{
public:
	network_result<tcp_prepared_statement> prepare_statement(
		tcp_connection& conn,
		std::string_view statement
	) override
	{
		return sync_exc_impl([&conn, statement] {
			return conn.prepare_statement(statement);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		value_list_it params_first,
		value_list_it params_last
	) override
	{
		return sync_exc_impl([&]{
			return stmt.execute(params_first, params_last);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		const std::vector<mysql::value>& values
	) override
	{
		return sync_exc_impl([&stmt, &values] {
			return stmt.execute(values);
		});
	}
};
sync_exc sync_exc_obj;

template <typename R, typename Callable>
network_result<R> async_impl(Callable&& cb) {
	std::promise<network_result<R>> prom;
	cb([&prom](error_code errc, error_info info, auto retval) {
		prom.set_value(network_result<R>{errc, std::move(info), std::move(retval)});
	});
	return prom.get_future().get();
}

class async : public network_functions
{
public:
	network_result<tcp_prepared_statement> prepare_statement(
		tcp_connection& conn,
		std::string_view statement
	) override
	{
		return async_impl<tcp_prepared_statement>([&conn, statement](auto&& token) {
			return conn.async_prepare_statement(statement, std::forward<decltype(token)>(token));
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		value_list_it params_first,
		value_list_it params_last
	) override
	{
		return async_impl<tcp_resultset>([&](auto&& token) {
			return stmt.async_execute(params_first, params_last, std::forward<decltype(token)>(token));
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		const std::vector<mysql::value>& values
	) override
	{
		return async_impl<tcp_resultset>([&](auto&& token) {
			return stmt.async_execute(values, std::forward<decltype(token)>(token));
		});
	}
};
async async_obj;

}

// Visible stuff
mysql::test::network_functions* mysql::test::sync_errc_network_functions = &sync_errc_obj;
mysql::test::network_functions* mysql::test::sync_exc_network_functions = &sync_exc_obj;
mysql::test::network_functions* mysql::test::async_network_functions = &async_obj;
