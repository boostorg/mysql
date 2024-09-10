//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/execution/blocking.hpp>
#include <boost/asio/execution/relationship.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/require.hpp>
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "test_common/tracker_executor.hpp"

using namespace boost::mysql::test;

namespace {

// Are we in the call stack of an initiating function?
thread_local bool g_is_running_initiation = false;

// Produce unique executor IDs (start in 1)
std::atomic_int next_executor_id{1};

// The executor call stack
thread_local std::vector<int> g_executor_call_stack;

// Guard to remove an entry from the stack
struct executor_call_stack_guard
{
    executor_call_stack_guard(int executor_id) { g_executor_call_stack.push_back(executor_id); }
    executor_call_stack_guard(const executor_call_stack_guard&) = delete;
    executor_call_stack_guard(executor_call_stack_guard&&) = delete;
    executor_call_stack_guard& operator=(const executor_call_stack_guard&) = delete;
    executor_call_stack_guard& operator=(executor_call_stack_guard&&) = delete;
    ~executor_call_stack_guard() { g_executor_call_stack.pop_back(); }
};

// Wraps a function object to insert tracking of the caller executor
template <class Function>
struct tracker_executor_function
{
    int executor_id;
    Function fn;

    void operator()()
    {
        executor_call_stack_guard guard(executor_id);
        std::move(fn)();
    }
};

template <class Function>
tracker_executor_function<typename std::decay<Function>::type> create_tracker_executor_function(
    int executor_id,
    Function&& fn
)
{
    return {executor_id, std::forward<Function>(fn)};
}

}  // namespace

namespace boost {
namespace mysql {
namespace test {

class tracker_executor
{
public:
    tracker_executor(int id, boost::asio::any_io_executor ex) noexcept : id_(id), ex_(std::move(ex)) {}

    bool operator==(const tracker_executor& rhs) const noexcept { return id_ == rhs.id_ && ex_ == rhs.ex_; }
    bool operator!=(const tracker_executor& rhs) const noexcept { return !(*this == rhs); }
    int id() const { return id_; }

#ifdef BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT

    // TS-executor interface
    asio::execution_context& context() const noexcept { return ex_.context(); }
    void on_work_started() const noexcept { ex_.on_work_started(); }
    void on_work_finished() const noexcept { ex_.on_work_finished(); }

    template <typename Function, typename Allocator>
    void dispatch(Function&& f, const Allocator& a) const
    {
        ex_.dispatch(create_tracker_executor_function(id_, std::forward<Function>(f)), a);
    }

    template <typename Function, typename Allocator>
    void post(Function&& f, const Allocator& a) const
    {
        ex_.post(create_tracker_executor_function(id_, std::forward<Function>(f)), a);
    }

    template <typename Function, typename Allocator>
    void defer(Function&& f, const Allocator& a) const
    {
        ex_.defer(create_tracker_executor_function(id_, std::forward<Function>(f)), a);
    }

#else

    // Standard executors interface
    template <class Property>
    tracker_executor require(
        const Property& p,
        typename std::enable_if<asio::can_require<asio::any_io_executor, Property>::value>::type* = nullptr
    ) const
    {
        return tracker_executor(id_, asio::require(ex_, p));
    }

    template <class Property>
    tracker_executor prefer(
        const Property& p,
        typename std::enable_if<asio::can_prefer<asio::any_io_executor, Property>::value>::type* = nullptr
    ) const
    {
        return tracker_executor(id_, asio::prefer(ex_, p));
    }

    template <class Property>
    auto query(
        const Property& p,
        typename std::enable_if<asio::can_query<asio::any_io_executor, Property>::value>::type* = nullptr
    ) const -> decltype(asio::query(std::declval<boost::asio::any_io_executor>(), p))
    {
        return boost::asio::query(ex_, p);
    }

    template <typename Function>
    void execute(Function&& f) const
    {
        ex_.execute(create_tracker_executor_function(id_, std::forward<Function>(f)));
    }
#endif

private:
    int id_;
    boost::asio::any_io_executor ex_;
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

// Asio fails to detect that a type is equaility comparable under MSVC, so we need to do this
namespace boost {
namespace asio {
namespace traits {

template <>
struct equality_comparable<tracker_executor>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;
};

template <typename Function>
struct execute_member<tracker_executor, Function>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;
    using result_type = void;
};

template <class Property>
struct require_member<
    tracker_executor,
    Property,
    typename std::enable_if<asio::can_require<asio::any_io_executor, Property>::value>::type>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;
    using result_type = tracker_executor;
};

template <typename Property>
struct prefer_member<
    tracker_executor,
    Property,
    typename std::enable_if<asio::can_prefer<asio::any_io_executor, Property>::value>::type>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;
    using result_type = tracker_executor;
};

template <typename Property>
struct query_member<
    tracker_executor,
    Property,
    typename std::enable_if<asio::can_query<asio::any_io_executor, Property>::value>::type>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;
    using result_type = decltype(boost::asio::query(
        std::declval<const any_io_executor&>(),
        std::declval<const Property&>()
    ));
};

}  // namespace traits
}  // namespace asio
}  // namespace boost

boost::mysql::test::tracker_executor_result boost::mysql::test::create_tracker_executor(
    asio::any_io_executor inner
)
{
    int id = ++next_executor_id;
    return {id, tracker_executor(id, std::move(inner))};
}

boost::span<const int> boost::mysql::test::executor_stack() { return g_executor_call_stack; }

int boost::mysql::test::get_executor_id(asio::any_io_executor ex)
{
    auto* typed_ex = ex.target<tracker_executor>();
    return typed_ex ? typed_ex->id() : -1;
}
boost::mysql::test::initiation_guard::initiation_guard()
{
    BOOST_ASSERT(!g_is_running_initiation);
    g_is_running_initiation = true;
}

boost::mysql::test::initiation_guard::~initiation_guard()
{
    BOOST_ASSERT(g_is_running_initiation);
    g_is_running_initiation = false;
}

bool boost::mysql::test::is_initiation_function() { return g_is_running_initiation; }

static boost::asio::io_context g_ctx;

boost::asio::any_io_executor boost::mysql::test::global_context_executor() { return g_ctx.get_executor(); }

void boost::mysql::test::run_global_context()
{
    g_ctx.restart();
    g_ctx.run();
}

void boost::mysql::test::poll_global_context(const bool* done)
{
    using std::chrono::steady_clock;

    // Restart the context, in case it was stopped
    g_ctx.restart();

    // Poll until this time point
    constexpr std::chrono::seconds timeout(5);
    auto timeout_tp = steady_clock::now() + timeout;

    // Perform the polling
    do
    {
        g_ctx.poll();
        std::this_thread::yield();
    } while (!*done && steady_clock::now() < timeout_tp);

    // Check for timeout
    BOOST_TEST_REQUIRE(*done);
}