//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include "network_functions_impl.hpp"
#include <boost/asio/use_future.hpp>
#include <boost/test/unit_test.hpp>

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
    void verify()
    {
        BOOST_TEST(call_count() == 1); // we call handler exactly once
        BOOST_TEST(calling_thread() != std::this_thread::get_id()); // handler runs in the io_context thread
    }
};

template <class Stream>
class async_callback_errinfo : public network_functions<Stream>
{
    template <class R>
    struct handler
    {
        std::promise<network_result<R>>& prom_;
        error_info& output_info_;
        handler_call_tracker& call_tracker_;

        // For operations with a return type
        void operator()(error_code code, R retval)
        {
            call_tracker_.register_call();
            prom_.set_value(network_result<R>(code, std::move(output_info_), std::move(retval)));
        }

        // For operations without a return type (R=no_result)
        void operator()(error_code code)
        {
            prom_.set_value(network_result<R>(code, std::move(output_info_)));
        }
    };

    template <class R, class Callable>
    network_result<R> impl(Callable&& cb)
    {
        handler_call_tracker call_tracker;
        std::promise<network_result<R>> prom;
        error_info info ("error_info not cleared properly");
        cb(handler<R>{prom, info, call_tracker}, info);
        auto res = prom.get_future().get();

        return res;
    }

public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override
    {
        return "async_callback_errinfo";
    }
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return conn.async_connect(ep, params, info, std::move(h));
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return conn.async_handshake(params, info, std::move(h));
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl<resultset_type>([&](handler<resultset_type> h, error_info& info) {
            return conn.async_query(query, info, std::move(h));
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl<prepared_statement_type>([&conn, statement](
                handler<prepared_statement_type> h, error_info& info) {
            return conn.async_prepare_statement(statement, info, std::move(h));
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl<resultset_type>([&](handler<resultset_type> h, error_info& info) {
            return stmt.async_execute(params_first, params_last, info, std::move(h));
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl<resultset_type>([&](handler<resultset_type> h, error_info& info) {
            return stmt.async_execute(values, info, std::move(h));
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return stmt.async_close(info, std::move(h));
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl<const row*>([&](handler<const row*> h, error_info& info) {
            return r.async_fetch_one(info, std::move(h));
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl<std::vector<owning_row>>([&](handler<std::vector<owning_row>> h, error_info& info) {
            return r.async_fetch_many(count, info, std::move(h));
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl<std::vector<owning_row>>([&](handler<std::vector<owning_row>> h, error_info& info) {
            return r.async_fetch_all(info, std::move(h));
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return conn.async_quit(info, std::move(h));
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return conn.async_close(info, std::move(h));
        });
    }
};

template <class Stream>
class async_callback_noerrinfo : public network_functions<Stream>
{
    template <class R>
    struct handler
    {
        std::promise<network_result<R>>& prom_;
        handler_call_tracker& call_tracker_;

        // For operations with a return type
        void operator()(error_code code, R retval)
        {
            call_tracker_.register_call();
            prom_.set_value(network_result<R>(code, std::move(retval)));
        }

        // For operations without a return type (R=no_result)
        void operator()(error_code code)
        {
            call_tracker_.register_call();
            prom_.set_value(network_result<R>(code));
        }
    };

    template <class R, class Callable>
    network_result<R> impl(Callable&& cb)
    {
        handler_call_tracker call_tracker;
        std::promise<network_result<R>> prom;
        cb(handler<R>{prom, call_tracker});
        return prom.get_future().get();
    }

public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override
    {
        return "async_callback_noerrinfo";
    }
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return conn.async_connect(ep, params, std::move(h));
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return conn.async_handshake(params, std::move(h));
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl<resultset_type>([&](handler<resultset_type> h) {
            return conn.async_query(query, std::move(h));
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl<prepared_statement_type>([&conn, statement](handler<prepared_statement_type> h) {
            return conn.async_prepare_statement(statement, std::move(h));
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl<resultset_type>([&](handler<resultset_type> h) {
            return stmt.async_execute(params_first, params_last, std::move(h));
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl<resultset_type>([&](handler<resultset_type> h) {
            return stmt.async_execute(values, std::move(h));
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return stmt.async_close(std::move(h));
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl<const row*>([&](handler<const row*> h) {
            return r.async_fetch_one(std::move(h));
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl<std::vector<owning_row>>([&](handler<std::vector<owning_row>> h) {
            return r.async_fetch_many(count, std::move(h));
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl<std::vector<owning_row>>([&](handler<std::vector<owning_row>> h) {
            return r.async_fetch_all(std::move(h));
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return conn.async_quit(std::move(h));
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return conn.async_close(std::move(h));
        });
    }
};

} // anon namespace

// Visible stuff
template <class Stream>
network_functions<Stream>* boost::mysql::test::async_callback_errinfo_functions()
{
    static async_callback_errinfo<Stream> res;
    return &res;
}

template <class Stream>
network_functions<Stream>* boost::mysql::test::async_callback_noerrinfo_functions()
{
    static async_callback_noerrinfo<Stream> res;
    return &res;
}

BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_callback_errinfo_functions)
BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_callback_noerrinfo_functions)

