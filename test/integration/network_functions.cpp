#include "network_functions.hpp"
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <future>

using namespace boost::mysql::test;
using boost::mysql::tcp_prepared_statement;
using boost::mysql::tcp_resultset;
using boost::mysql::tcp_connection;
using boost::mysql::error_info;
using boost::mysql::error_code;
using boost::mysql::detail::make_error_code;
using boost::mysql::errc;
using boost::mysql::value;
using boost::mysql::row;
using boost::mysql::owning_row;
using boost::asio::yield_context;
using boost::asio::use_future;

namespace
{

// A non-empty error_info to verify that we correctly clear the message
error_info make_initial_error_info()
{
	return error_info("Error info not cleared correctly");
}

error_code make_initial_error_code()
{
	return make_error_code(errc::no);
}

class sync_errc : public network_functions
{
	template <typename Callable>
	static auto impl(Callable&& cb) {
		using R = decltype(cb(std::declval<error_code&>(), std::declval<error_info&>()));
		network_result<R> res (make_initial_error_code(), make_initial_error_info());
		res.value = cb(res.err, *res.info);
		return res;
	}
public:
	const char* name() const override { return "sync_errc"; }
	network_result<no_result> handshake(
		tcp_connection& conn,
		const boost::mysql::connection_params& params
	) override
	{
		return impl([&](error_code& code, error_info& info) {
			conn.handshake(params, code, info);
			return no_result();
		});
	}
	network_result<tcp_resultset> query(
		tcp_connection& conn,
		std::string_view query
	) override
	{
		return impl([&](error_code& code, error_info& info) {
			return conn.query(query, code, info);
		});
	}
	network_result<tcp_prepared_statement> prepare_statement(
		tcp_connection& conn,
		std::string_view statement
	) override
	{
		return impl([&conn, statement](error_code& err, error_info& info) {
			return conn.prepare_statement(statement, err, info);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		value_list_it params_first,
		value_list_it params_last
	) override
	{
		return impl([=, &stmt](error_code& err, error_info& info) {
			return stmt.execute(params_first, params_last, err, info);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		const std::vector<value>& values
	) override
	{
		return impl([&stmt, &values](error_code& err, error_info& info) {
			return stmt.execute(values, err, info);
		});
	}
	network_result<no_result> close_statement(
		tcp_prepared_statement& stmt
	) override
	{
		return impl([&](error_code& code, error_info& info) {
			stmt.close(code, info);
			return no_result();
		});
	}
	network_result<const row*> fetch_one(
		tcp_resultset& r
	) override
	{
		return impl([&](error_code& code, error_info& info) {
			return r.fetch_one(code, info);
		});
	}
	network_result<std::vector<owning_row>> fetch_many(
		tcp_resultset& r,
		std::size_t count
	) override
	{
		return impl([&](error_code& code, error_info& info) {
			return r.fetch_many(count, code, info);
		});
	}
	network_result<std::vector<owning_row>> fetch_all(
		tcp_resultset& r
	) override
	{
		return impl([&](error_code& code, error_info& info) {
			return r.fetch_all(code, info);
		});
	}
};

class sync_exc : public network_functions
{
	template <typename Callable>
	static auto impl(Callable&& cb) {
		using R = decltype(cb());
		network_result<R> res;
		try
		{
			res.value = cb();
		}
		catch (const boost::system::system_error& err)
		{
			res.err = err.code();
			res.info = error_info(err.what());
		}
		return res;
	}
public:
	const char* name() const override { return "sync_exc"; }
	network_result<no_result> handshake(
		tcp_connection& conn,
		const boost::mysql::connection_params& params
	) override
	{
		return impl([&] {
			conn.handshake(params);
			return no_result();
		});
	}
	network_result<tcp_resultset> query(
		tcp_connection& conn,
		std::string_view query
	) override
	{
		return impl([&] {
			return conn.query(query);
		});
	}
	network_result<tcp_prepared_statement> prepare_statement(
		tcp_connection& conn,
		std::string_view statement
	) override
	{
		return impl([&conn, statement] {
			return conn.prepare_statement(statement);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		value_list_it params_first,
		value_list_it params_last
	) override
	{
		return impl([&]{
			return stmt.execute(params_first, params_last);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		const std::vector<value>& values
	) override
	{
		return impl([&stmt, &values] {
			return stmt.execute(values);
		});
	}
	network_result<no_result> close_statement(
		tcp_prepared_statement& stmt
	) override
	{
		return impl([&] {
			stmt.close();
			return no_result();
		});
	}
	network_result<const row*> fetch_one(
		tcp_resultset& r
	) override
	{
		return impl([&] {
			return r.fetch_one();
		});
	}
	network_result<std::vector<owning_row>> fetch_many(
		tcp_resultset& r,
		std::size_t count
	) override
	{
		return impl([&] {
			return r.fetch_many(count);
		});
	}
	network_result<std::vector<owning_row>> fetch_all(
		tcp_resultset& r
	) override
	{
		return impl([&] {
			return r.fetch_all();
		});
	}
};

class async_callback : public network_functions
{
	template <typename R, typename Callable>
	static network_result<R> impl(Callable&& cb)
	{
		struct handler
		{
			std::promise<network_result<R>>& prom;
			error_info& info;

			// For operations with a return type
			void operator()(error_code code, R retval)
			{
				prom.set_value(network_result<R>(
					code,
					std::move(info),
					std::move(retval)
				));
			}

			// For operations without a return type (R=no_result)
			void operator()(error_code code)
			{
				prom.set_value(network_result<R>(
					code,
					std::move(info)
				));
			}
		};

		std::promise<network_result<R>> prom;
		error_info info = make_initial_error_info();
		cb(handler{prom, info}, &info);
		return prom.get_future().get();
	}

public:
	const char* name() const override { return "async_callback"; }
	network_result<no_result> handshake(
		tcp_connection& conn,
		const boost::mysql::connection_params& params
	) override
	{
		return impl<no_result>([&](auto&& token, error_info* info) {
			return conn.async_handshake(params, std::forward<decltype(token)>(token), info);
		});
	}
	network_result<tcp_resultset> query(
		tcp_connection& conn,
		std::string_view query
	) override
	{
		return impl<tcp_resultset>([&](auto&& token, error_info* info) {
			return conn.async_query(query, std::forward<decltype(token)>(token), info);
		});
	}
	network_result<tcp_prepared_statement> prepare_statement(
		tcp_connection& conn,
		std::string_view statement
	) override
	{
		return impl<tcp_prepared_statement>([&conn, statement](auto&& token, error_info* info) {
			return conn.async_prepare_statement(statement, std::forward<decltype(token)>(token), info);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		value_list_it params_first,
		value_list_it params_last
	) override
	{
		return impl<tcp_resultset>([&](auto&& token, error_info* info) {
			return stmt.async_execute(params_first, params_last, std::forward<decltype(token)>(token), info);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		const std::vector<value>& values
	) override
	{
		return impl<tcp_resultset>([&](auto&& token, error_info* info) {
			return stmt.async_execute(values, std::forward<decltype(token)>(token), info);
		});
	}
	network_result<no_result> close_statement(
		tcp_prepared_statement& stmt
	) override
	{
		return impl<no_result>([&](auto&& token, error_info* info) {
			return stmt.async_close(std::forward<decltype(token)>(token), info);
		});
	}
	network_result<const row*> fetch_one(
		tcp_resultset& r
	) override
	{
		return impl<const row*>([&](auto&& token, error_info* info) {
			return r.async_fetch_one(std::forward<decltype(token)>(token), info);
		});
	}
	network_result<std::vector<owning_row>> fetch_many(
		tcp_resultset& r,
		std::size_t count
	) override
	{
		return impl<std::vector<owning_row>>([&](auto&& token, error_info* info) {
			return r.async_fetch_many(count, std::forward<decltype(token)>(token), info);
		});
	}
	network_result<std::vector<owning_row>> fetch_all(
		tcp_resultset& r
	) override
	{
		return impl<std::vector<owning_row>>([&](auto&& token, error_info* info) {
			return r.async_fetch_all(std::forward<decltype(token)>(token), info);
		});
	}
};

class async_coroutine : public network_functions
{
	template <typename IoObj, typename Callable>
	static auto impl(IoObj& obj, Callable&& cb) {
		using R = decltype(cb(
			std::declval<yield_context>(),
			std::declval<error_info*>()
		));

		std::promise<network_result<R>> prom;

		boost::asio::spawn(obj.next_layer().get_executor(), [&](yield_context yield) {
			error_code ec = make_initial_error_code();
			error_info info = make_initial_error_info();
			R result = cb(yield[ec], &info);
			prom.set_value(network_result<R>(
				ec,
				std::move(info),
				std::move(result)
			));
		});

		return prom.get_future().get();
	}

public:
	const char* name() const override { return "async_coroutine"; }
	network_result<no_result> handshake(
		tcp_connection& conn,
		const boost::mysql::connection_params& params
	) override
	{
		return impl(conn, [&](yield_context yield, error_info* info) {
			conn.async_handshake(params, yield, info);
			return no_result();
		});
	}
	network_result<tcp_resultset> query(
		tcp_connection& conn,
		std::string_view query
	) override
	{
		return impl(conn, [&](yield_context yield, error_info* info) {
			return conn.async_query(query, yield, info);
		});
	}
	network_result<tcp_prepared_statement> prepare_statement(
		tcp_connection& conn,
		std::string_view statement
	) override
	{
		return impl(conn, [&](yield_context yield, error_info* info) {
			return conn.async_prepare_statement(statement, yield, info);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		value_list_it params_first,
		value_list_it params_last
	) override
	{
		return impl(stmt, [&](yield_context yield, error_info* info) {
			return stmt.async_execute(params_first, params_last, yield, info);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		const std::vector<value>& values
	) override
	{
		return impl(stmt, [&](yield_context yield, error_info* info) {
			return stmt.async_execute(values, yield, info);
		});
	}
	network_result<no_result> close_statement(
		tcp_prepared_statement& stmt
	) override
	{
		return impl(stmt, [&](yield_context yield, error_info* info) {
			stmt.async_close(yield, info);
			return no_result();
		});
	}
	network_result<const row*> fetch_one(
		tcp_resultset& r
	) override
	{
		return impl(r, [&](yield_context yield, error_info* info) {
			return r.async_fetch_one(yield, info);
		});
	}
	network_result<std::vector<owning_row>> fetch_many(
		tcp_resultset& r,
		std::size_t count
	) override
	{
		return impl(r, [&](yield_context yield, error_info* info) {
			return r.async_fetch_many(count, yield, info);
		});
	}
	network_result<std::vector<owning_row>> fetch_all(
		tcp_resultset& r
	) override
	{
		return impl(r, [&](yield_context yield, error_info* info) {
			return r.async_fetch_all(yield, info);
		});
	}
};


class async_future : public network_functions
{
	template <typename Callable>
	static auto impl(Callable&& cb) {
		using R = decltype(cb().get()); // Callable returns a future<R>
		std::future<R> fut = cb();
		try
		{
			// error_info is not available here, so we skip validation
			return network_result<R>(
				error_code(),
				fut.get()
			);
		}
		catch (const boost::system::system_error& err)
		{
			// error_info is not available here, so we skip validation
			return network_result<R>(err.code());
		}
	}

	template <typename Callable>
	static network_result<no_result> impl_no_result(Callable&& cb) {
		std::future<void> fut = cb();
		try
		{
			// error_info is not available here, so we skip validation
			fut.get();
			return network_result<no_result>(error_code());
		}
		catch (const boost::system::system_error& err)
		{
			// error_info is not available here, so we skip validation
			return network_result<no_result>(err.code());
		}
	}
public:
	const char* name() const override { return "async_future"; }
	network_result<no_result> handshake(
		tcp_connection& conn,
		const boost::mysql::connection_params& params
	) override
	{
		return impl_no_result([&] {
			return conn.async_handshake(params, use_future);
		});
	}
	network_result<tcp_resultset> query(
		tcp_connection& conn,
		std::string_view query
	) override
	{
		return impl([&] {
			return conn.async_query(query, use_future);
		});
	}
	network_result<tcp_prepared_statement> prepare_statement(
		tcp_connection& conn,
		std::string_view statement
	) override
	{
		return impl([&]{
			return conn.async_prepare_statement(statement, use_future);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		value_list_it params_first,
		value_list_it params_last
	) override
	{
		return impl([&]{
			return stmt.async_execute(params_first, params_last, use_future);
		});
	}
	network_result<tcp_resultset> execute_statement(
		tcp_prepared_statement& stmt,
		const std::vector<value>& values
	) override
	{
		return impl([&] {
			return stmt.async_execute(values, use_future);
		});
	}
	network_result<no_result> close_statement(
		tcp_prepared_statement& stmt
	) override
	{
		return impl_no_result([&] {
			return stmt.async_close(use_future);
		});
	}
	network_result<const row*> fetch_one(
		tcp_resultset& r
	) override
	{
		return impl([&] {
			return r.async_fetch_one(use_future);
		});
	}
	network_result<std::vector<owning_row>> fetch_many(
		tcp_resultset& r,
		std::size_t count
	) override
	{
		return impl([&] {
			return r.async_fetch_many(count, use_future);
		});
	}
	network_result<std::vector<owning_row>> fetch_all(
		tcp_resultset& r
	) override
	{
		return impl([&] {
			return r.async_fetch_all(use_future);
		});
	}
};

// Global objects to be exposed
sync_errc sync_errc_obj;
sync_exc sync_exc_obj;
async_callback async_callback_obj;
async_coroutine async_coroutine_obj;
async_future async_future_obj;

}

// Visible stuff
boost::mysql::test::network_functions* boost::mysql::test::sync_errc_network_functions = &sync_errc_obj;
boost::mysql::test::network_functions* boost::mysql::test::sync_exc_network_functions = &sync_exc_obj;
boost::mysql::test::network_functions* boost::mysql::test::async_callback_network_functions = &async_callback_obj;
boost::mysql::test::network_functions* boost::mysql::test::async_coroutine_network_functions = &async_coroutine_obj;
boost::mysql::test::network_functions* boost::mysql::test::async_future_network_functions = &async_future_obj;
