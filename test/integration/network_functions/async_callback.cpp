//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include "network_functions_impl.hpp"
#include <boost/asio/use_future.hpp>

using namespace boost::mysql::test;
using boost::mysql::connection_params;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::value;
using boost::mysql::row;
using boost::mysql::owning_row;

namespace
{

class handler_call_tracker
{
    int call_count_ {};
    std::thread::id calling_thread_ {};
public:
    handler_call_tracker() = default;
    void register_call()
    {
        ++call_count_;
        calling_thread_ = std::this_thread::get_id();
    }
    int call_count() const { return call_count_; }
    std::thread::id calling_thread() const { return calling_thread_; }
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
            handler_call_tracker& call_tracker_;

            // For operations with a return type
            void operator()(error_code code, R retval)
            {
                call_tracker_.register_call();
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
                call_tracker_.register_call();
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

        handler_call_tracker call_tracker;
        std::promise<network_result<R>> prom;
        error_info info ("error_info not cleared properly");
        error_info* infoptr = use_errinfo_ ? &info : nullptr;
        cb(handler{prom, infoptr, call_tracker}, infoptr);
        auto res = prom.get_future().get();
        EXPECT_EQ(call_tracker.call_count(), 1); // we call handler exactly once
        EXPECT_NE(call_tracker.calling_thread(), std::this_thread::get_id()); // handler runs in the io_context thread
        return res;
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
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl<no_result>([&](auto&& token, error_info* info) {
            return conn.async_connect(ep, params, std::forward<decltype(token)>(token), info);
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
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

} // anon namespace

// Visible stuff
template <typename Stream>
network_functions<Stream>* boost::mysql::test::async_callback_errinfo_functions()
{
    static async_callback<Stream> res (true);
    return &res;
}

template <typename Stream>
network_functions<Stream>* boost::mysql::test::async_callback_noerrinfo_functions()
{
    static async_callback<Stream> res (false);
    return &res;
}

BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_callback_errinfo_functions)
BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_callback_noerrinfo_functions)

