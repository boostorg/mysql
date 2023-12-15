//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/mysql/impl/internal/connection_pool/connection_node.hpp>
#include <boost/mysql/impl/internal/connection_pool/connection_pool_impl.hpp>
#include <boost/mysql/impl/internal/connection_pool/internal_pool_params.hpp>
#include <boost/mysql/impl/internal/connection_pool/sansio_connection_node.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/test/tools/interface.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <utility>

#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_common/tracker_executor.hpp"
#include "test_unit/pool_printing.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
namespace asio = boost::asio;
using boost::mysql::client_errc;
using boost::mysql::common_server_errc;
using boost::mysql::connect_params;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
using boost::mysql::pool_params;
using boost::mysql::pooled_connection;
using std::chrono::steady_clock;

BOOST_AUTO_TEST_SUITE(test_connection_pool_impl)

class mock_timer_service : public asio::execution_context::service
{
public:
    void shutdown() final {}
    static const asio::execution_context::id id;

    mock_timer_service(asio::execution_context& owner) : asio::execution_context::service(owner) {}

    struct pending_timer
    {
        steady_clock::time_point expiry;
        asio::any_completion_handler<void(error_code)> handler;
        asio::any_io_executor timer_ex;
        int timer_id;
    };

    struct cancel_handler
    {
        mock_timer_service* svc;
        std::list<pending_timer>::iterator it;

        void operator()(asio::cancellation_type_t) { svc->fire_timer(it, asio::error::operation_aborted); }
    };

    void add_timer(pending_timer&& t)
    {
        if (t.expiry <= current_time_)
        {
            call_handler(std::move(t), error_code());
        }
        else
        {
            pending_.push_front(std::move(t));
            auto slot = asio::get_associated_cancellation_slot(pending_.front().handler);
            if (slot.is_connected())
            {
                slot.emplace<cancel_handler>(cancel_handler{this, pending_.begin()});
            }
        }
    }

    std::size_t cancel(int timer_id)
    {
        std::size_t num_cancels = 0;
        for (auto it = pending_.begin(); it != pending_.end();)
        {
            if (it->timer_id == timer_id)
            {
                ++num_cancels;
                it = fire_timer(it, asio::error::operation_aborted);
            }
            else
            {
                ++it;
            }
        }
        return num_cancels;
    }

    void advance_time_to(steady_clock::time_point new_time)
    {
        for (auto it = pending_.begin(); it != pending_.end();)
        {
            if (it->expiry <= new_time)
                it = fire_timer(it, error_code());
            else
                ++it;
        }
        current_time_ = new_time;
    }

    void advance_time_by(steady_clock::duration by) { advance_time_to(current_time_ + by); }

    int allocate_timer_id() { return ++current_timer_id_; }

    steady_clock::time_point current_time() const noexcept { return current_time_; }

private:
    std::list<pending_timer> pending_;
    steady_clock::time_point current_time_;
    int current_timer_id_{0};

    void call_handler(pending_timer&& t, error_code ec)
    {
        asio::post(std::move(t.timer_ex), asio::append(std::move(t.handler), ec));
    }

    std::list<pending_timer>::iterator fire_timer(std::list<pending_timer>::iterator it, error_code ec)
    {
        auto t = std::move(*it);
        auto res = pending_.erase(it);
        call_handler(std::move(t), ec);
        return res;
    }
};

const asio::execution_context::id mock_timer_service::id;

class mock_timer
{
    mock_timer_service* svc_;
    int timer_id_;
    asio::any_io_executor ex_;
    steady_clock::time_point expiry_;

    struct initiate_wait
    {
        template <class Handler>
        void operator()(Handler&& h, mock_timer* self)
        {
            self->svc_->add_timer({self->expiry_, std::forward<Handler>(h), self->ex_, self->timer_id_});
        }
    };

public:
    mock_timer(asio::any_io_executor ex)
        : svc_(&asio::use_service<mock_timer_service>(ex.context())),
          timer_id_(svc_->allocate_timer_id()),
          ex_(std::move(ex))
    {
    }

    asio::any_io_executor get_executor() { return ex_; }

    void expires_at(steady_clock::time_point new_expiry)
    {
        svc_->cancel(timer_id_);
        expiry_ = new_expiry;
    }

    void expires_after(steady_clock::duration dur) { expires_at(svc_->current_time() + dur); }

    std::size_t cancel() { return svc_->cancel(timer_id_); }

    template <class CompletionToken>
    auto async_wait(CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_wait(),
            token,
            std::declval<mock_timer*>()
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(initiate_wait(), token, this);
    }
};

class mock_connection
{
    asio::experimental::channel<void(error_code, next_connection_action)> recv_chan_;
    asio::experimental::channel<void(error_code, diagnostics)> send_chan_;

    struct run_op
    {
        mock_connection& obj;
        next_connection_action act;
        diagnostics* diag;

        template <class Self>
        void operator()(Self& self)
        {
            obj.recv_chan_.async_send(error_code(), act, std::move(self));
        }

        template <class Self>
        void operator()(Self& self, error_code ec)
        {
            if (ec)
            {
                self.complete(ec);
            }
            else
            {
                obj.send_chan_.async_receive(std::move(self));
            }
        }

        template <class Self>
        void operator()(Self& self, error_code ec, diagnostics recv_diag)
        {
            if (diag)
                *diag = std::move(recv_diag);
            self.complete(ec);
        }
    };

    struct step_op
    {
        mock_connection& obj;
        next_connection_action expected_act;
        error_code op_ec;
        diagnostics op_diag;

        template <class Self>
        void operator()(Self& self)
        {
            obj.recv_chan_.async_receive(std::move(self));
        }

        template <class Self>
        void operator()(Self& self, error_code ec, next_connection_action actual_act)
        {
            BOOST_TEST(ec == error_code());
            BOOST_TEST(actual_act == expected_act);
            obj.send_chan_.async_send(op_ec, std::move(op_diag), std::move(self));
        }

        template <class Self>
        void operator()(Self& self, error_code ec)
        {
            self.complete(ec);
        }
    };

    template <class CompletionToken>
    auto op_impl(next_connection_action act, diagnostics* diag, CompletionToken&& token)
        -> decltype(asio::async_compose<CompletionToken, void(error_code)>(
            std::declval<run_op>(),
            token,
            asio::any_io_executor()
        ))
    {
        return asio::async_compose<CompletionToken, void(error_code)>(
            run_op{*this, act, diag},
            token,
            recv_chan_.get_executor()
        );
    }

public:
    boost::mysql::any_connection_params ctor_params;
    boost::mysql::connect_params last_connect_params;

    mock_connection(asio::any_io_executor ex, boost::mysql::any_connection_params ctor_params)
        : recv_chan_(ex), send_chan_(std::move(ex)), ctor_params(ctor_params)
    {
    }

    template <class CompletionToken>
    auto async_connect(const connect_params* params, diagnostics& diag, CompletionToken&& token)
        -> decltype(op_impl(next_connection_action{}, &diag, std::forward<CompletionToken>(token)))
    {
        BOOST_TEST(params != nullptr);
        last_connect_params = *params;
        return op_impl(next_connection_action::connect, &diag, std::forward<CompletionToken>(token));
    }

    template <class CompletionToken>
    auto async_ping(CompletionToken&& token)
        -> decltype(op_impl(next_connection_action{}, nullptr, std::forward<CompletionToken>(token)))
    {
        return op_impl(next_connection_action::ping, nullptr, std::forward<CompletionToken>(token));
    }

    template <class CompletionToken>
    auto async_reset_connection(CompletionToken&& token)
        -> decltype(op_impl(next_connection_action{}, nullptr, std::forward<CompletionToken>(token)))
    {
        return op_impl(next_connection_action::reset, nullptr, std::forward<CompletionToken>(token));
    }

    template <class CompletionToken>
    void step(next_connection_action act, CompletionToken&& token, error_code ec = {}, diagnostics diag = {})
    {
        return asio::async_compose<CompletionToken, void(error_code)>(
            step_op{*this, act, ec, std::move(diag)},
            token,
            send_chan_
        );
    }
};

struct mock_io_traits
{
    using connection_type = mock_connection;
    using timer_type = mock_timer;
};

using mock_node = basic_connection_node<mock_io_traits>;

struct mock_pooled_connection;

using mock_pool = basic_pool_impl<mock_io_traits, mock_pooled_connection>;

struct mock_pooled_connection
{
    std::shared_ptr<mock_pool> pool;
    mock_node* node{};

    mock_pooled_connection() = default;
    mock_pooled_connection(mock_node& node, std::shared_ptr<mock_pool> pool) noexcept
        : pool(std::move(pool)), node(&node)
    {
    }
};

using mock_shared_state = conn_shared_state<mock_io_traits>;

// Issue posts until a certain condition becomes true (with a sane limit)
static void post_until(std::function<bool()> cond, std::function<void()> handler)
{
    struct op
    {
        std::function<bool()> cond;
        std::function<void()> handler;
        int i{0};

        op(std::function<bool()> cond, std::function<void()> handler)
            : cond(std::move(cond)), handler(std::move(handler))
        {
        }

        void operator()()
        {
            if (cond())
            {
                handler();
            }
            else if (i++ >= 10)
            {
                BOOST_TEST_REQUIRE(false);
                handler();
            }
            else
            {
                asio::post(std::move(*this));
            }
        }
    };

    op(std::move(cond), std::move(handler))();
}

static void wait_for_status(const mock_node& node, connection_status status, std::function<void()> handler)
{
    post_until([&node, status] { return node.status() == status; }, std::move(handler));
}

class detached_get_connection
{
    asio::experimental::channel<void(error_code, mock_pooled_connection)> chan;
    mock_pool& pool_;
    executor_info exec_info_;

public:
    detached_get_connection(
        mock_pool& pool,
        std::chrono::steady_clock::duration timeout,
        diagnostics* diag = nullptr
    )
        : chan(pool.get_executor()), pool_(pool)
    {
        auto ex = create_tracker_executor(chan.get_executor(), &exec_info_);
        pool.async_get_connection(
            asio::use_service<mock_timer_service>(ex.context()).current_time() + timeout,
            diag,
            asio::bind_executor(
                ex,
                [&](error_code ec, mock_pooled_connection c) {
                    BOOST_TEST(exec_info_.total() > 0u);
                    chan.async_send(ec, std::move(c), asio::detached);
                }
            )
        );
    }

    void wait(mock_node& expected_node, asio::yield_context yield)
    {
        error_code ec;
        auto conn = chan.async_receive(yield[ec]);
        BOOST_TEST(ec == error_code());
        BOOST_TEST(conn.node == &expected_node);
        BOOST_TEST(conn.pool.get() == &pool_);
    }

    void wait(error_code expected_ec, asio::yield_context yield)
    {
        error_code ec;
        auto conn = chan.async_receive(yield[ec]);
        BOOST_TEST(ec == expected_ec);
        BOOST_TEST(conn.node == nullptr);
        BOOST_TEST(conn.pool.get() == nullptr);
    }
};

static void pool_test(
    boost::mysql::pool_params params,
    std::function<void(mock_pool&, std::function<void()>)> test_fun
)
{
    // I/O context
    asio::io_context ctx;

    // Pool (must be created using dynamic memory)
    auto pool = std::make_shared<mock_pool>(ctx, std::move(params));

    // This flag is only set to true after the test finishes.
    // If the test timeouts, it will be false
    bool finished = false;

    // Run the test
    post_until(
        // Wait until a connection is created (common to all tests)
        [&] { return !pool->nodes().empty(); },
        [&] {
            // Invoke the test
            test_fun(*pool, [&] {
                // Finish
                pool->cancel();
                finished = true;
            });
        }
    );

    // Run the pool
    pool->async_run([](error_code ec) { BOOST_TEST(ec == error_code()); });

    // If the test doesn't complete in this time, there was an error
    ctx.run_for(std::chrono::seconds(10));

    // Check that we didn't timeout
    BOOST_TEST(finished == true);
}

static mock_timer_service& get_timer_service(mock_pool& pool)
{
    return asio::use_service<mock_timer_service>(pool.get_executor().context());
}

static void check_shared_st(
    mock_pool& pool,
    error_code expected_ec,
    const diagnostics& expected_diag,
    std::size_t expected_num_pending,
    std::size_t expected_num_idle
)
{
    const auto& st = pool.shared_state();
    BOOST_TEST(st.last_ec == expected_ec);
    BOOST_TEST(st.last_diag == expected_diag);
    BOOST_TEST(st.num_pending_connections == expected_num_pending);
    BOOST_TEST(st.idle_list.size() == expected_num_idle);
}

// connection lifecycle
BOOST_AUTO_TEST_CASE(lifecycle_connect_error)
{
    pool_params params;
    params.retry_interval = std::chrono::seconds(2);

    struct op : asio::coroutine
    {
        mock_pool* pool;
        std::function<void()> handler;

        void operator()(mock_pool& p, std::function<void()> h)
        {
            pool = &p;
            handler = std::move(h);
            (*this)();
        }

        void operator()(error_code ec = {})
        {
            BOOST_TEST(ec == error_code());
            auto& node = pool->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Connection trying to connect
                BOOST_ASIO_CORO_YIELD
                wait_for_status(node, connection_status::connect_in_progress, std::move(*this));
                check_shared_st(*pool, error_code(), diagnostics(), 1, 0);

                // Connect fails, so the connection goes to sleep. Diagnostics are stored in shared state.
                BOOST_ASIO_CORO_YIELD
                node.connection().step(
                    next_connection_action::connect,
                    std::move(*this),
                    common_server_errc::er_aborting_connection,
                    create_server_diag("Connection error!")
                );
                BOOST_ASIO_CORO_YIELD
                wait_for_status(node, connection_status::sleep_connect_failed_in_progress, std::move(*this));
                check_shared_st(
                    *pool,
                    common_server_errc::er_aborting_connection,
                    create_server_diag("Connection error!"),
                    1,
                    0
                );

                // Advance until it's time to retry again
                get_timer_service(*pool).advance_time_by(std::chrono::seconds(2));
                BOOST_ASIO_CORO_YIELD
                wait_for_status(node, connection_status::sleep_connect_failed_in_progress, std::move(*this));
                check_shared_st(
                    *pool,
                    common_server_errc::er_aborting_connection,
                    create_server_diag("Connection error!"),
                    1,
                    0
                );

                // Connection connects successfully this time. Diagnostics have
                // been cleared and the connection is marked as idle
                BOOST_ASIO_CORO_YIELD
                node.connection().step(next_connection_action::connect, std::move(*this), error_code());
                BOOST_ASIO_CORO_YIELD
                wait_for_status(node, connection_status::idle, std::move(*this));
                check_shared_st(*pool, error_code(), diagnostics(), 0, 1);

                // Done
                handler();
            }
        }
    };

    pool_test(std::move(params), op());
}

// BOOST_AUTO_TEST_CASE(lifecycle_connect_timeout)
// {
//     pool_params params;
//     params.connect_timeout = std::chrono::seconds(5);
//     params.retry_interval = std::chrono::seconds(2);

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Connection trying to connect
//         auto& node = pool.nodes().front();
//         wait_for_status(node, connection_status::connect_in_progress, yield);

//         // Timeout ellapses. Connect is considered failed
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(5));
//         wait_for_status(node, connection_status::sleep_connect_failed_in_progress, yield);
//         check_shared_st(pool, client_errc::timeout, diagnostics(), 1, 0);

//         // Advance until it's time to retry again
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(2));
//         wait_for_status(node, connection_status::sleep_connect_failed_in_progress, yield);
//         check_shared_st(pool, client_errc::timeout, diagnostics(), 1, 0);

//         // Connection connects successfully this time
//         node.connection().step(next_connection_action::connect, yield, error_code());
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_return_without_reset)
// {
//     pool_params params;

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Wait until a connection is successfully connected
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);

//         // Simulate a user picking the connection
//         node.mark_as_in_use();
//         check_shared_st(pool, error_code(), diagnostics(), 0, 0);

//         // Simulate a user returning the connection (without reset)
//         node.mark_as_collectable(false);

//         // The connection goes back to idle without invoking resets
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_reset_success)
// {
//     pool_params params;

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Wait until a connection is successfully connected, then pick it up
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         node.mark_as_in_use();

//         // Simulate a user returning the connection (with reset)
//         node.mark_as_collectable(true);

//         // A reset is issued
//         wait_for_status(node, connection_status::reset_in_progress, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 1, 0);

//         // Successful reset makes the connection idle again
//         node.connection().step(next_connection_action::reset, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_reset_error)
// {
//     pool_params params;

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Connect, pick up and return a connection
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         node.mark_as_in_use();
//         node.mark_as_collectable(true);
//         wait_for_status(node, connection_status::reset_in_progress, yield);

//         // Reset fails. This triggers a reconnection. Diagnostics are not saved
//         node.connection()
//             .step(next_connection_action::reset, yield, common_server_errc::er_aborting_connection);
//         wait_for_status(node, connection_status::connect_in_progress, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 1, 0);

//         // Reconnect succeeds. We're idle again
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_reset_timeout)
// {
//     pool_params params;
//     params.ping_timeout = std::chrono::seconds(1);

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Connect, pick up and return a connection
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         node.mark_as_in_use();
//         node.mark_as_collectable(true);
//         wait_for_status(node, connection_status::reset_in_progress, yield);

//         // Reset times out. This triggers a reconnection
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(1));
//         wait_for_status(node, connection_status::connect_in_progress, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 1, 0);

//         // Reconnect succeeds. We're idle again
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_reset_timeout_disabled)
// {
//     pool_params params;
//     params.ping_timeout = std::chrono::seconds(0);

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Connect, pick up and return a connection
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         node.mark_as_in_use();
//         node.mark_as_collectable(true);
//         wait_for_status(node, connection_status::reset_in_progress, yield);

//         // Reset doesn't time out, regardless of how much time we wait
//         get_timer_service(pool).advance_time_by(std::chrono::hours(9999));
//         asio::post(yield);
//         BOOST_TEST(node.status() == connection_status::reset_in_progress);
//         check_shared_st(pool, error_code(), diagnostics(), 1, 0);

//         // Reset succeeds
//         node.connection().step(next_connection_action::reset, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_ping_success)
// {
//     pool_params params;
//     params.ping_interval = std::chrono::seconds(100);

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Wait until a connection is successfully connected
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);

//         // Wait until ping interval ellapses. This triggers a ping
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(100));
//         wait_for_status(node, connection_status::ping_in_progress, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 1, 0);

//         // After ping succeeds, connection goes back to idle
//         node.connection().step(next_connection_action::ping, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_ping_error)
// {
//     pool_params params;
//     params.ping_interval = std::chrono::seconds(100);

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Wait until a connection is successfully connected
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);

//         // Wait until ping interval ellapses
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(100));

//         // Ping fails. This triggers a reconnection. Diagnostics are not saved
//         node.connection()
//             .step(next_connection_action::ping, yield, common_server_errc::er_aborting_connection);
//         wait_for_status(node, connection_status::connect_in_progress, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 1, 0);

//         // Reconnection succeeds
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_ping_timeout)
// {
//     pool_params params;
//     params.ping_interval = std::chrono::seconds(100);
//     params.ping_timeout = std::chrono::seconds(2);

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Wait until a connection is successfully connected
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);

//         // Wait until ping interval ellapses
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(100));
//         wait_for_status(node, connection_status::ping_in_progress, yield);

//         // Ping times out. This triggers a reconnection. Diagnostics are not saved
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(2));
//         wait_for_status(node, connection_status::connect_in_progress, yield);

//         // Reconnection succeeds
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_ping_timeout_disabled)
// {
//     pool_params params;
//     params.ping_interval = std::chrono::seconds(100);
//     params.ping_timeout = std::chrono::seconds(0);

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Wait until a connection is successfully connected
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);

//         // Wait until ping interval ellapses
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(100));
//         wait_for_status(node, connection_status::ping_in_progress, yield);

//         // Ping doesn't time out, regardless of how much we wait
//         get_timer_service(pool).advance_time_by(std::chrono::hours(9999));
//         asio::post(yield);
//         BOOST_TEST(node.status() == connection_status::ping_in_progress);

//         // Ping succeeds
//         node.connection().step(next_connection_action::ping, yield);
//         wait_for_status(node, connection_status::idle, yield);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// BOOST_AUTO_TEST_CASE(lifecycle_ping_disabled)
// {
//     pool_params params;
//     params.ping_interval = std::chrono::seconds(0);

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Wait until a connection is successfully connected
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);

//         // Connection won't ping, regardless of how much time we wait
//         get_timer_service(pool).advance_time_by(std::chrono::hours(9999));
//         asio::post(yield);
//         BOOST_TEST(node.status() == connection_status::idle);
//         check_shared_st(pool, error_code(), diagnostics(), 0, 1);
//     });
// }

// // async_get_connection
// BOOST_AUTO_TEST_CASE(get_connection_wait_success)
// {
//     pool_params params;
//     params.retry_interval = std::chrono::seconds(2);

//     pool_test(std::move(params), [&](asio::yield_context yield, mock_pool& pool) {
//         auto& node = pool.nodes().front();

//         // Connection tries to connect and fails
//         node.connection()
//             .step(next_connection_action::connect, yield, common_server_errc::er_aborting_connection);
//         wait_for_status(node, connection_status::sleep_connect_failed_in_progress, yield);

//         // A request for a connection is issued. The request doesn't find
//         // any available connection, and the current one is pending, so no new connections are created
//         detached_get_connection task(pool, std::chrono::seconds(5), nullptr);
//         post_until([&] { return pool.num_pending_requests() > 0u; }, yield);
//         BOOST_TEST(pool.nodes().size() == 1u);

//         // Retry interval ellapses and connection retries and succeeds
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(2));
//         node.connection().step(next_connection_action::connect, yield);

//         // Request is fulfilled
//         task.wait(node, yield);
//         BOOST_TEST(node.status() == connection_status::in_use);
//         BOOST_TEST(pool.nodes().size() == 1u);
//         BOOST_TEST(pool.num_pending_requests() == 0u);
//     });
// }

// BOOST_AUTO_TEST_CASE(get_connection_wait_timeout_no_diag)
// {
//     pool_test(pool_params{}, [](asio::yield_context yield, mock_pool& pool) {
//         // A request for a connection is issued. The request doesn't find
//         // any available connection, and the current one is pending, so no new connections are created
//         diagnostics diag;
//         detached_get_connection task(pool, std::chrono::seconds(1), &diag);
//         post_until([&] { return pool.num_pending_requests() > 0u; }, yield);
//         BOOST_TEST(pool.nodes().size() == 1u);

//         // The request timeout ellapses, so the request fails
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(1));
//         task.wait(client_errc::timeout, yield);
//         BOOST_TEST(diag == diagnostics());
//         BOOST_TEST(pool.nodes().size() == 1u);
//         BOOST_TEST(pool.num_pending_requests() == 0u);
//     });
// }

// BOOST_AUTO_TEST_CASE(get_connection_wait_timeout_with_diag)
// {
//     pool_test(pool_params{}, [](asio::yield_context yield, mock_pool& pool) {
//         // A request for a connection is issued. The request doesn't find
//         // any available connection, and the current one is pending, so no new connections are created
//         diagnostics diag;
//         detached_get_connection task(pool, std::chrono::seconds(1), &diag);
//         post_until([&] { return pool.num_pending_requests() > 0u; }, yield);
//         BOOST_TEST(pool.nodes().size() == 1u);

//         // The connection fails to connect
//         pool.nodes().begin()->connection().step(
//             next_connection_action::connect,
//             yield,
//             common_server_errc::er_bad_db_error,
//             create_server_diag("Bad db")
//         );

//         // The request timeout ellapses, so the request fails
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(1));
//         task.wait(common_server_errc::er_bad_db_error, yield);
//         BOOST_TEST(diag == create_server_diag("Bad db"));
//         BOOST_TEST(pool.nodes().size() == 1u);
//         BOOST_TEST(pool.num_pending_requests() == 0u);
//     });
// }

// BOOST_AUTO_TEST_CASE(get_connection_wait_timeout_with_diag_nullptr)
// {
//     // We don't crash if diag is nullptr
//     pool_test(pool_params{}, [](asio::yield_context yield, mock_pool& pool) {
//         // A request for a connection is issued. The request doesn't find
//         // any available connection, and the current one is pending, so no new connections are created
//         detached_get_connection task(pool, std::chrono::seconds(1), nullptr);
//         post_until([&] { return pool.num_pending_requests() > 0u; }, yield);
//         BOOST_TEST(pool.nodes().size() == 1u);

//         // The connection fails to connect
//         pool.nodes().begin()->connection().step(
//             next_connection_action::connect,
//             yield,
//             common_server_errc::er_bad_db_error,
//             create_server_diag("Bad db")
//         );

//         // The request timeout ellapses, so the request fails
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(1));
//         task.wait(common_server_errc::er_bad_db_error, yield);
//         BOOST_TEST(pool.nodes().size() == 1u);
//         BOOST_TEST(pool.num_pending_requests() == 0u);
//     });
// }

// BOOST_AUTO_TEST_CASE(get_connection_immediate_completion)
// {
//     pool_test(pool_params{}, [&](asio::yield_context yield, mock_pool& pool) {
//         // Wait for a connection to be ready
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node, connection_status::idle, yield);

//         // A request for a connection is issued. The request completes immediately
//         detached_get_connection(pool, std::chrono::seconds(5), nullptr).wait(node, yield);
//         BOOST_TEST(node.status() == connection_status::in_use);
//         BOOST_TEST(pool.nodes().size() == 1u);
//         BOOST_TEST(pool.num_pending_requests() == 0u);
//     });
// }

// BOOST_AUTO_TEST_CASE(get_connection_connection_creation)
// {
//     pool_params params;
//     params.initial_size = 1;
//     params.max_size = 2;

//     pool_test(std::move(params), [&](asio::yield_context yield, mock_pool& pool) {
//         // Wait for a connection to be ready, then get it from the pool
//         auto& node1 = pool.nodes().front();
//         node1.connection().step(next_connection_action::connect, yield);
//         wait_for_status(node1, connection_status::idle, yield);
//         detached_get_connection(pool, std::chrono::seconds(5), nullptr).wait(node1, yield);

//         // Another request is issued. The connection we have is in use, so another one is created.
//         // Since this is not immediate, the task will need to wait
//         detached_get_connection task2(pool, std::chrono::seconds(5), nullptr);
//         post_until([&] { return pool.nodes().size() == 2u; }, yield);
//         auto& node2 = *std::next(pool.nodes().begin());
//         BOOST_TEST(pool.num_pending_requests() == 1u);

//         // Connection connects successfully and is handed to us
//         node2.connection().step(next_connection_action::connect, yield);
//         task2.wait(node2, yield);
//         BOOST_TEST(node2.status() == connection_status::in_use);
//         BOOST_TEST(pool.nodes().size() == 2u);
//         BOOST_TEST(pool.num_pending_requests() == 0u);

//         // Another request is issued. All connections are in use but max size is already
//         // reached, so no new connection is created
//         detached_get_connection task3(pool, std::chrono::seconds(5), nullptr);
//         post_until([&] { return pool.num_pending_requests() == 1u; }, yield);
//         BOOST_TEST(pool.nodes().size() == 2u);

//         // When one of the connections is returned, the request is fulfilled
//         node2.mark_as_collectable(false);
//         task3.wait(node2, yield);
//         BOOST_TEST(pool.num_pending_requests() == 0u);
//         BOOST_TEST(pool.nodes().size() == 2u);
//     });
// }

// BOOST_AUTO_TEST_CASE(get_connection_multiple_requests)
// {
//     pool_params params;
//     params.initial_size = 2;
//     params.max_size = 2;

//     pool_test(std::move(params), [&](asio::yield_context yield, mock_pool& pool) {
//         // Issue some parallel requests
//         detached_get_connection task1(pool, std::chrono::seconds(5), nullptr);
//         detached_get_connection task2(pool, std::chrono::seconds(5), nullptr);
//         detached_get_connection task3(pool, std::chrono::seconds(5), nullptr);
//         detached_get_connection task4(pool, std::chrono::seconds(2), nullptr);
//         detached_get_connection task5(pool, std::chrono::seconds(5), nullptr);

//         // Two connections can be created. These fulfill two requests
//         post_until([&] { return pool.nodes().size() == 2u; }, yield);
//         auto& node1 = pool.nodes().front();
//         auto& node2 = *std::next(pool.nodes().begin());
//         node1.connection().step(next_connection_action::connect, yield);
//         node2.connection().step(next_connection_action::connect, yield);
//         task1.wait(node1, yield);
//         task2.wait(node2, yield);

//         // Time ellapses and task4 times out
//         get_timer_service(pool).advance_time_by(std::chrono::seconds(2));
//         task4.wait(client_errc::timeout, yield);

//         // A connection is returned. The first task to enter is served
//         node1.mark_as_collectable(true);
//         node1.connection().step(next_connection_action::reset, yield);
//         task3.wait(node1, yield);

//         // The next connection to be returned is for task5
//         node2.mark_as_collectable(false);
//         task5.wait(node2, yield);

//         // Done
//         BOOST_TEST(pool.num_pending_requests() == 0u);
//         BOOST_TEST(pool.nodes().size() == 2u);
//     });
// }

// BOOST_AUTO_TEST_CASE(get_connection_cancel)
// {
//     pool_test(pool_params{}, [&](asio::yield_context yield, mock_pool& pool) {
//         // Issue some requests
//         detached_get_connection task1(pool, std::chrono::seconds(5), nullptr);
//         detached_get_connection task2(pool, std::chrono::seconds(5), nullptr);
//         post_until([&] { return pool.num_pending_requests() == 2u; }, yield);

//         // While in flight, cancel the pool
//         pool.cancel();

//         // All tasks fail with a cancelled code
//         task1.wait(client_errc::cancelled, yield);
//         task2.wait(client_errc::cancelled, yield);

//         // Further tasks fail immediately
//         detached_get_connection(pool, std::chrono::seconds(5), nullptr).wait(client_errc::cancelled,
//         yield);
//     });
// }

// // pool_params have the intended effect
// BOOST_AUTO_TEST_CASE(params_ssl_ctx_buffsize)
// {
//     // Pass a custom ssl context and buffer size
//     pool_params params;
//     params.ssl_ctx.emplace(boost::asio::ssl::context::tlsv12_client);
//     params.initial_read_buffer_size = 16u;

//     // SSL context matching is performed using the underlying handle
//     // because ssl::context provides no way to query the options previously set
//     auto handle = params.ssl_ctx->native_handle();

//     pool_test(std::move(params), [&](asio::yield_context, mock_pool& pool) {
//         auto ctor_params = pool.nodes().front().connection().ctor_params;
//         BOOST_TEST_REQUIRE(ctor_params.ssl_context != nullptr);
//         BOOST_TEST(ctor_params.ssl_context->native_handle() == handle);
//         BOOST_TEST(ctor_params.initial_read_buffer_size == 16u);
//     });
// }

// BOOST_AUTO_TEST_CASE(params_connect_1)
// {
//     pool_params params;
//     params.server_address.emplace_host_and_port("myhost", 1234);
//     params.username = "myuser";
//     params.password = "mypasswd";
//     params.database = "mydb";
//     params.ssl = boost::mysql::ssl_mode::disable;
//     params.multi_queries = true;

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Connect
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);

//         // Check params
//         const auto& cparams = node.connection().last_connect_params;
//         BOOST_TEST(cparams.connection_collation == std::uint16_t(0));
//         BOOST_TEST(cparams.server_address.hostname() == "myhost");
//         BOOST_TEST(cparams.server_address.port() == std::uint16_t(1234));
//         BOOST_TEST(cparams.username == "myuser");
//         BOOST_TEST(cparams.password == "mypasswd");
//         BOOST_TEST(cparams.database == "mydb");
//         BOOST_TEST(cparams.ssl == boost::mysql::ssl_mode::disable);
//         BOOST_TEST(cparams.multi_queries == true);
//     });
// }

// BOOST_AUTO_TEST_CASE(params_connect_2)
// {
//     pool_params params;
//     params.server_address.emplace_unix_path("/mysock");
//     params.username = "myuser2";
//     params.password = "mypasswd2";
//     params.database = "mydb2";
//     params.ssl = boost::mysql::ssl_mode::require;
//     params.multi_queries = false;

//     pool_test(std::move(params), [](asio::yield_context yield, mock_pool& pool) {
//         // Connect
//         auto& node = pool.nodes().front();
//         node.connection().step(next_connection_action::connect, yield);

//         // Check params
//         const auto& cparams = node.connection().last_connect_params;
//         BOOST_TEST(cparams.connection_collation == std::uint16_t(0));
//         BOOST_TEST(cparams.server_address.unix_socket_path() == "/mysock");
//         BOOST_TEST(cparams.username == "myuser2");
//         BOOST_TEST(cparams.password == "mypasswd2");
//         BOOST_TEST(cparams.database == "mydb2");
//         BOOST_TEST(cparams.ssl == boost::mysql::ssl_mode::require);
//         BOOST_TEST(cparams.multi_queries == false);
//     });
// }

BOOST_AUTO_TEST_SUITE_END()
