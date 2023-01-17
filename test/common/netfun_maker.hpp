//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_NETFUN_MAKER_HPP
#define BOOST_MYSQL_TEST_COMMON_NETFUN_MAKER_HPP

#include <boost/mysql/server_errc.hpp>
#include <boost/mysql/server_error.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>

#include <cstddef>
#include <functional>
#include <type_traits>

#include "network_result.hpp"

namespace boost {
namespace mysql {
namespace test {

namespace test_detail {

class set_err_token_callback
{
    error_code* err_;

public:
    set_err_token_callback(error_code& err) noexcept : err_(&err) {}
    void operator()(error_code ec) const noexcept { *err_ = ec; }
};

template <class T>
class set_err_token_callback_1
{
    error_code* err_;
    T* ptr_;

public:
    set_err_token_callback_1(error_code& err, T& output) noexcept : err_(&err), ptr_(&output) {}
    void operator()(error_code ec, T v) const
    {
        *err_ = ec;
        *ptr_ = v;
    }
};

class tracker_executor
{
    boost::asio::io_context::executor_type ex_;
    std::size_t* num_posts_{};

public:
    tracker_executor(boost::asio::io_context& ctx, std::size_t& num_posts)
        : ex_(ctx.get_executor()), num_posts_(&num_posts)
    {
    }

    boost::asio::io_context& context() const noexcept { return ex_.context(); }

    void on_work_started() const { ex_.on_work_started(); }
    void on_work_finished() const { ex_.on_work_finished(); }

    template <typename Function, typename OtherAllocator>
    void dispatch(Function&& f, const OtherAllocator& a) const
    {
        ex_.dispatch(std::forward<Function>(f), a);
    }

    template <typename Function, typename OtherAllocator>
    void defer(Function&& f, const OtherAllocator& a) const
    {
        ex_.defer(std::forward<Function>(f), a);
    }

    template <typename Function, typename OtherAllocator>
    void post(Function&& f, const OtherAllocator& a) const
    {
        ++*num_posts_;
        ex_.post(std::forward<Function>(f), a);
    }
};

template <class T>
inline auto create_tracker_token(
    boost::asio::io_context& ctx,
    std::size_t& num_posts,
    network_result<T>& result
)
    -> decltype(boost::asio::bind_executor(
        std::declval<tracker_executor>(),
        std::declval<set_err_token_callback_1<T>>()
    ))
{
    return boost::asio::bind_executor(
        tracker_executor(ctx, num_posts),
        set_err_token_callback_1<T>(result.err, result.value)
    );
}

inline auto create_tracker_token(
    boost::asio::io_context& ctx,
    std::size_t& num_posts,
    network_result<void>& result
)
    -> decltype(boost::asio::bind_executor(
        std::declval<tracker_executor>(),
        std::declval<set_err_token_callback>()
    ))
{
    return boost::asio::bind_executor(
        tracker_executor(ctx, num_posts),
        set_err_token_callback(result.err)
    );
}

template <class T>
using tracker_token_t = decltype(create_tracker_token(
    std::declval<boost::asio::io_context&>(),
    std::declval<std::size_t&>(),
    std::declval<network_result<T>&>()
));

template <class Fn, class... Args>
auto my_invoke(Fn fn, Args&&... args) -> typename std::enable_if<
    std::is_function<typename std::remove_pointer<Fn>::type>::value,
    decltype(fn(std::forward<Args>(args)...))>::type
{
    return fn(std::forward<Args>(args)...);
}

template <class Pmem, class Obj, class... Args>
auto my_invoke(Pmem fn, Obj& obj, Args&&... args) -> typename std::enable_if<
    std::is_member_function_pointer<Pmem>::value,
    decltype((obj.*fn)(std::forward<Args>(args)...))>::type
{
    return (obj.*fn)(std::forward<Args>(args)...);
}

template <class T, class... InvokeArgs>
void invoke_and_assign(network_result<T>& output, InvokeArgs&&... args)
{
    output.value = my_invoke(std::forward<InvokeArgs>(args)...);
}

template <class... InvokeArgs>
void invoke_and_assign(network_result<void>&, InvokeArgs&&... args)
{
    my_invoke(std::forward<InvokeArgs>(args)...);
}

template <class R, class... Args>
struct netfun_maker_impl
{
    using signature = std::function<network_result<R>(Args...)>;

    template <class Pfn>
    static signature sync_errc(Pfn fn)
    {
        return [fn](Args... args) {
            network_result<R> res(
                boost::mysql::make_error_code(server_errc::no),
                server_diagnostics("server_diagnostics not cleared properly")
            );
            invoke_and_assign(res, fn, std::forward<Args>(args)..., res.err, *res.diag);
            return res;
        };
    }

    template <class Pfn>
    static signature sync_exc(Pfn fn)
    {
        return [fn](Args... args) {
            network_result<R> res;
            try
            {
                invoke_and_assign(res, fn, std::forward<Args>(args)...);
            }
            catch (const boost::mysql::server_error& err)
            {
                res.err = err.code();
                res.diag = err.diagnostics();
            }
            catch (const boost::system::system_error& err)
            {
                res.err = err.code();
            }
            return res;
        };
    }

    template <class Pfn>
    static signature async_errinfo(Pfn fn)
    {
        return [fn](Args... args) {
            boost::asio::io_context ctx;
            std::size_t num_posts = 0;
            network_result<R> res(
                boost::mysql::make_error_code(server_errc::no),
                server_diagnostics("server_diagnostics not cleared properly")
            );
            my_invoke(
                fn,
                std::forward<Args>(args)...,
                *res.diag,
                create_tracker_token(ctx, num_posts, res)
            );
            ctx.run();
            BOOST_TEST(num_posts > 0u);
            return res;
        };
    }

    template <class Pfn>
    static signature async_noerrinfo(Pfn fn)
    {
        return [fn](Args... args) {
            boost::asio::io_context ctx;
            std::size_t num_posts = 0;
            network_result<R> res(boost::mysql::make_error_code(server_errc::no));
            my_invoke(fn, std::forward<Args>(args)..., create_tracker_token(ctx, num_posts, res));
            ctx.run();
            BOOST_TEST(num_posts > 0u);
            return res;
        };
    }
};

}  // namespace test_detail

template <class R, class Obj, class... Args>
class netfun_maker_mem
{
    using impl = test_detail::netfun_maker_impl<R, Obj&, Args...>;

public:
    using signature = std::function<network_result<R>(Obj&, Args...)>;
    using isig_sync_errc = R (Obj::*)(Args..., error_code&, server_diagnostics&);
    using isig_sync_exc = R (Obj::*)(Args...);
    using isig_async_errinfo =
        void (Obj::*)(Args..., server_diagnostics&, test_detail::tracker_token_t<R>&&);
    using isig_async_noerrinfo = void (Obj::*)(Args..., test_detail::tracker_token_t<R>&&);

    static signature sync_errc(isig_sync_errc pfn) { return impl::sync_errc(pfn); }
    static signature sync_exc(isig_sync_exc pfn) { return impl::sync_exc(pfn); }
    static signature async_errinfo(isig_async_errinfo pfn) { return impl::async_errinfo(pfn); }
    static signature async_noerrinfo(isig_async_noerrinfo pfn)
    {
        return impl::async_noerrinfo(pfn);
    }
};

template <class R, class... Args>
class netfun_maker_fn
{
    using impl = test_detail::netfun_maker_impl<R, Args...>;

public:
    using signature = std::function<network_result<R>(Args...)>;
    using isig_sync_errc = R (*)(Args..., error_code&, server_diagnostics&);
    using isig_sync_exc = R (*)(Args...);
    using isig_async_errinfo =
        void (*)(Args..., server_diagnostics&, test_detail::tracker_token_t<R>&&);
    using isig_async_noerrinfo = void (*)(Args..., test_detail::tracker_token_t<R>&&);

    static signature sync_errc(isig_sync_errc pfn) { return impl::sync_errc(pfn); }
    static signature sync_exc(isig_sync_exc pfn) { return impl::sync_exc(pfn); }
    static signature async_errinfo(isig_async_errinfo pfn) { return impl::async_errinfo(pfn); }
    static signature async_noerrinfo(isig_async_noerrinfo pfn)
    {
        return impl::async_noerrinfo(pfn);
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
