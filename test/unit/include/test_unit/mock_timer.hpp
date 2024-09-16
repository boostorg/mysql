//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_MOCK_TIMER_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_MOCK_TIMER_HPP

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/system/detail/error_code.hpp>

#include <chrono>

namespace boost {
namespace mysql {
namespace test {

// Like steady_clock, but the current time can be set within tests
struct mock_clock
{
    using rep = typename std::chrono::steady_clock::rep;
    using period = typename std::chrono::steady_clock::period;
    using duration = typename std::chrono::steady_clock::duration;
    using time_point = typename std::chrono::steady_clock::time_point;
    static constexpr bool is_steady = true;
    static time_point now();
};

// Advance mock_clock and call relevant handlers for timers associated with the given context
void advance_time_by(asio::execution_context& ctx, std::chrono::steady_clock::duration dur);

// Helper typedef
using mock_timer = asio::basic_waitable_timer<mysql::test::mock_clock>;

// Forward decl
class mock_timer_service;

}  // namespace test
}  // namespace mysql
}  // namespace boost

namespace boost {
namespace asio {

// Defining this as a specialization allows for better compatibility with other Asio code.
// Using just a custom WaitTraits object doesn't suit, because Asio will still use its internal
// timer service, which makes tests less predictable
template <>
class basic_waitable_timer<mysql::test::mock_clock>
{
    using this_type = basic_waitable_timer<mysql::test::mock_clock>;

    mysql::test::mock_timer_service* svc_;
    int timer_id_;
    any_io_executor ex_;
    std::chrono::steady_clock::time_point expiry_;

    void add_to_service(any_completion_handler<void(system::error_code)> handler);

    struct initiate_wait
    {
        template <class Handler>
        void operator()(Handler&& h, this_type* self)
        {
            self->add_to_service(std::forward<Handler>(h));
        }
    };

public:
    basic_waitable_timer(asio::any_io_executor ex);

    basic_waitable_timer(asio::any_io_executor ex, std::chrono::steady_clock::time_point tp)
        : basic_waitable_timer(std::move(ex))
    {
        expires_at(tp);
    }

    asio::any_io_executor get_executor() { return ex_; }
    std::size_t expires_at(std::chrono::steady_clock::time_point new_expiry);
    std::size_t expires_after(std::chrono::steady_clock::duration dur);
    std::size_t cancel();

    template <class CompletionToken>
    auto async_wait(CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(system::error_code)>(
            initiate_wait(),
            token,
            std::declval<this_type*>()
        ))
    {
        return asio::async_initiate<CompletionToken, void(system::error_code)>(initiate_wait(), token, this);
    }
};

}  // namespace asio
}  // namespace boost

#endif
