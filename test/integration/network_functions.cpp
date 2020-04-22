//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "network_functions.hpp"
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <future>

using namespace boost::mysql::test;
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

template <typename Stream>
class sync_errc : public network_functions<Stream>
{
    template <typename Callable>
    static auto impl(Callable&& cb) {
        using R = decltype(cb(std::declval<error_code&>(), std::declval<error_info&>()));
        network_result<R> res (make_initial_error_code(), make_initial_error_info());
        res.value = cb(res.err, *res.info);
        return res;
    }
public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override { return "sync_errc"; }
    network_result<no_result> handshake(
        connection_type& conn,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            conn.handshake(params, code, info);
            return no_result();
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        std::string_view query
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            return conn.query(query, code, info);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        std::string_view statement
    ) override
    {
        return impl([&conn, statement](error_code& err, error_info& info) {
            return conn.prepare_statement(statement, err, info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl([=, &stmt](error_code& err, error_info& info) {
            return stmt.execute(params_first, params_last, err, info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl([&stmt, &values](error_code& err, error_info& info) {
            return stmt.execute(values, err, info);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            stmt.close(code, info);
            return no_result();
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            return r.fetch_one(code, info);
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            return r.fetch_many(count, code, info);
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            return r.fetch_all(code, info);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            conn.quit(code, info);
            return no_result();
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            conn.close(code, info);
            return no_result();
        });
    }
};

template <typename Stream>
class sync_exc : public network_functions<Stream>
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
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override { return "sync_exc"; }
    network_result<no_result> handshake(
        connection_type& conn,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl([&] {
            conn.handshake(params);
            return no_result();
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        std::string_view query
    ) override
    {
        return impl([&] {
            return conn.query(query);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        std::string_view statement
    ) override
    {
        return impl([&conn, statement] {
            return conn.prepare_statement(statement);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl([&]{
            return stmt.execute(params_first, params_last);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl([&stmt, &values] {
            return stmt.execute(values);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl([&] {
            stmt.close();
            return no_result();
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl([&] {
            return r.fetch_one();
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl([&] {
            return r.fetch_many(count);
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl([&] {
            return r.fetch_all();
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl([&] {
            conn.quit();
            return no_result();
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl([&] {
            conn.close();
            return no_result();
        });
    }
};

template <typename Stream>
class async_callback : public network_functions<Stream>
{
    // This allows for two versions of the tests: one where we pass a
    // non-nullptr error_info* to the initiating function, and another
    // one where we pass nullptr.
    bool use_errinfo_;

    template <typename R, typename Callable>
    network_result<R> impl(Callable&& cb)
    {
        struct handler
        {
            std::promise<network_result<R>>& prom;
            error_info* info; // nullptr for !use_errinfo_

            // For operations with a return type
            void operator()(error_code code, R retval)
            {
                if (info)
                {
                    prom.set_value(network_result<R>(code, std::move(*info), std::move(retval)));
                }
                else
                {
                    prom.set_value(network_result<R>(code, std::move(retval)));
                }
            }

            // For operations without a return type (R=no_result)
            void operator()(error_code code)
            {
                if (info)
                {
                    prom.set_value(network_result<R>(code, std::move(*info)));
                }
                else
                {
                    prom.set_value(network_result<R>(code));
                }
            }
        };

        std::promise<network_result<R>> prom;
        error_info info = make_initial_error_info();
        error_info* infoptr = use_errinfo_ ? &info : nullptr;
        cb(handler{prom, infoptr}, infoptr);
        return prom.get_future().get();
    }

public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    async_callback(bool use_errinfo): use_errinfo_(use_errinfo) {}
    const char* name() const override
    {
        return use_errinfo_ ? "async_callback_errinfo" : "async_callback_noerrinfo";
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl<no_result>([&](auto&& token, error_info* info) {
            return conn.async_handshake(params, std::forward<decltype(token)>(token), info);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        std::string_view query
    ) override
    {
        return impl<resultset_type>([&](auto&& token, error_info* info) {
            return conn.async_query(query, std::forward<decltype(token)>(token), info);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        std::string_view statement
    ) override
    {
        return impl<prepared_statement_type>([&conn, statement](auto&& token, error_info* info) {
            return conn.async_prepare_statement(statement, std::forward<decltype(token)>(token), info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl<resultset_type>([&](auto&& token, error_info* info) {
            return stmt.async_execute(params_first, params_last, std::forward<decltype(token)>(token), info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl<resultset_type>([&](auto&& token, error_info* info) {
            return stmt.async_execute(values, std::forward<decltype(token)>(token), info);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl<no_result>([&](auto&& token, error_info* info) {
            return stmt.async_close(std::forward<decltype(token)>(token), info);
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl<const row*>([&](auto&& token, error_info* info) {
            return r.async_fetch_one(std::forward<decltype(token)>(token), info);
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl<std::vector<owning_row>>([&](auto&& token, error_info* info) {
            return r.async_fetch_many(count, std::forward<decltype(token)>(token), info);
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl<std::vector<owning_row>>([&](auto&& token, error_info* info) {
            return r.async_fetch_all(std::forward<decltype(token)>(token), info);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl<no_result>([&](auto&& token, error_info* info) {
            return conn.async_quit(std::forward<decltype(token)>(token), info);
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl<no_result>([&](auto&& token, error_info* info) {
            return conn.async_close(std::forward<decltype(token)>(token), info);
        });
    }
};

template <typename Stream>
class async_coroutine : public network_functions<Stream>
{
    bool use_errinfo_;

    template <typename IoObj, typename Callable>
    auto impl(IoObj& obj, Callable&& cb) {
        using R = decltype(cb(
            std::declval<yield_context>(),
            std::declval<error_info*>()
        ));

        std::promise<network_result<R>> prom;

        boost::asio::spawn(obj.next_layer().get_executor(), [&, this](yield_context yield) {
            error_code ec = make_initial_error_code();
            error_info info = make_initial_error_info();
            R result = cb(yield[ec], use_errinfo_ ? &info: nullptr);
            if (use_errinfo_)
            {
                prom.set_value(network_result<R>(ec, std::move(info), std::move(result)));
            }
            else
            {
                prom.set_value(network_result<R>(ec, std::move(result)));
            }
        });

        return prom.get_future().get();
    }

public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    async_coroutine(bool use_errinfo): use_errinfo_(use_errinfo) {}
    const char* name() const override
    {
        return use_errinfo_ ? "async_coroutine_errinfo" : "async_coroutine_noerrinfo";
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info* info) {
            conn.async_handshake(params, yield, info);
            return no_result();
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        std::string_view query
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info* info) {
            return conn.async_query(query, yield, info);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        std::string_view statement
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info* info) {
            return conn.async_prepare_statement(statement, yield, info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl(stmt, [&](yield_context yield, error_info* info) {
            return stmt.async_execute(params_first, params_last, yield, info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl(stmt, [&](yield_context yield, error_info* info) {
            return stmt.async_execute(values, yield, info);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl(stmt, [&](yield_context yield, error_info* info) {
            stmt.async_close(yield, info);
            return no_result();
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl(r, [&](yield_context yield, error_info* info) {
            return r.async_fetch_one(yield, info);
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl(r, [&](yield_context yield, error_info* info) {
            return r.async_fetch_many(count, yield, info);
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl(r, [&](yield_context yield, error_info* info) {
            return r.async_fetch_all(yield, info);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info* info) {
            conn.async_quit(yield, info);
            return no_result();
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info* info) {
            conn.async_close(yield, info);
            return no_result();
        });
    }
};

template <typename Stream>
class async_future : public network_functions<Stream>
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
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override { return "async_future_noerrinfo"; }
    network_result<no_result> handshake(
        connection_type& conn,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl_no_result([&] {
            return conn.async_handshake(params, use_future);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        std::string_view query
    ) override
    {
        return impl([&] {
            return conn.async_query(query, use_future);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        std::string_view statement
    ) override
    {
        return impl([&]{
            return conn.async_prepare_statement(statement, use_future);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl([&]{
            return stmt.async_execute(params_first, params_last, use_future);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl([&] {
            return stmt.async_execute(values, use_future);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl_no_result([&] {
            return stmt.async_close(use_future);
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl([&] {
            return r.async_fetch_one(use_future);
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl([&] {
            return r.async_fetch_many(count, use_future);
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl([&] {
            return r.async_fetch_all(use_future);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl_no_result([&] {
            return conn.async_quit(use_future);
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl_no_result([&] {
            return conn.async_close(use_future);
        });
    }
};

}

// Visible stuff
template <typename Stream>
boost::mysql::test::network_function_array<Stream>
boost::mysql::test::make_all_network_functions()
{
    static sync_errc<Stream> sync_errc_obj;
    static sync_exc<Stream> sync_exc_obj;
    static async_callback<Stream> async_callback_errinfo_obj (true);
    static async_callback<Stream> async_callback_noerrinfo_obj (false);
    static async_coroutine<Stream> async_coroutine_errinfo_obj (true);
    static async_coroutine<Stream> async_coroutine_noerrinfo_obj (false);
    static async_future<Stream> async_future_obj;

    return {
        &sync_errc_obj,
        &sync_exc_obj,
        &async_callback_errinfo_obj,
        &async_callback_noerrinfo_obj,
        &async_coroutine_errinfo_obj,
        &async_coroutine_noerrinfo_obj,
        &async_future_obj
    };
}

template boost::mysql::test::network_function_array<boost::asio::ip::tcp::socket>
boost::mysql::test::make_all_network_functions();

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
template boost::mysql::test::network_function_array<boost::asio::local::stream_protocol::socket>
boost::mysql::test::make_all_network_functions();
#endif
