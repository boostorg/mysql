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
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstddef>
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

/**
 * These tests verify step-by-step that all interactions between
 * elements in connection_pool work as intended. They use the templating
 * on IoTraits to mock out I/O objects. This allows for fast and reliable tests.
 * Boost.Asio lacks testing infrastructure, so we've coded some here.
 * These are complex tests.
 */

BOOST_AUTO_TEST_SUITE(test_connection_pool_impl)

/**
 * Used by mock timers. Like asio::detail::deadline_timer_service, but for mock timers.
 * Mock timers don't rely on the actual clock, but on a time_point hold by this class.
 * Call mock_timer_service::advance_time_xx to adjust the current time. This will
 * call timer handlers as if time had advanced. Note that we don't have a way to mock
 * steady_clock::now(). Our code under test must make sure not to call it.
 */
class mock_timer_service : public asio::execution_context::service
{
public:
    // Required by all Boost.Asio services
    void shutdown() final {}
    static const asio::execution_context::id id;
    mock_timer_service(asio::execution_context& owner) : asio::execution_context::service(owner) {}

    // A pending timer
    struct pending_timer
    {
        // When does the timer expire?
        steady_clock::time_point expiry;

        // The executor to use
        asio::any_io_executor ex;

        // What handler should we call?
        asio::any_completion_handler<void(error_code)> handler;

        // Uniquely identifies the timer, so we can implement cancellation
        int timer_id;
    };

    // Used by timer's wait initiation
    void add_timer(pending_timer&& t)
    {
        if (t.expiry <= current_time_)
        {
            // If the timer's expiry is in the past, directly call the handler
            post_handler(std::move(t), error_code());
        }
        else
        {
            // Add the timer op into the queue
            pending_.push_front(std::move(t));

            // Enable cancellation
            auto slot = asio::get_associated_cancellation_slot(pending_.front().handler);
            if (slot.is_connected())
            {
                slot.emplace<cancel_handler>(cancel_handler{this, pending_.begin()});
            }
        }
    }

    // Cancel all ops for the given timer_id
    std::size_t cancel(int timer_id)
    {
        std::size_t num_cancels = 0;
        for (auto it = pending_.begin(); it != pending_.end();)
        {
            if (it->timer_id == timer_id)
            {
                ++num_cancels;
                it = erase_and_post_handler(it, asio::error::operation_aborted);
            }
            else
            {
                ++it;
            }
        }
        return num_cancels;
    }

    // Set the new current time, calling handlers in the process
    void advance_time_to(steady_clock::time_point new_time)
    {
        for (auto it = pending_.begin(); it != pending_.end();)
        {
            if (it->expiry <= new_time)
                it = erase_and_post_handler(it, error_code());
            else
                ++it;
        }
        current_time_ = new_time;
    }

    // Same, but with a duration
    void advance_time_by(steady_clock::duration by) { advance_time_to(current_time_ + by); }

    // Used by timers, to retrieve their timer id
    int allocate_timer_id() { return ++current_timer_id_; }

    steady_clock::time_point current_time() const noexcept { return current_time_; }

private:
    std::list<pending_timer> pending_;
    steady_clock::time_point current_time_;
    int current_timer_id_{0};

    struct cancel_handler
    {
        mock_timer_service* svc;
        std::list<pending_timer>::iterator it;

        void operator()(asio::cancellation_type_t)
        {
            svc->erase_and_post_handler(it, asio::error::operation_aborted);
        }
    };

    // Schedule the handler to be called
    void post_handler(pending_timer&& t, error_code ec)
    {
        asio::get_associated_cancellation_slot(t.handler).clear();
        asio::post(std::move(t.ex), asio::append(std::move(t.handler), ec));
    }

    // Same, but also removes the op from the list
    std::list<pending_timer>::iterator erase_and_post_handler(
        std::list<pending_timer>::iterator it,
        error_code ec
    )
    {
        auto t = std::move(*it);
        auto res = pending_.erase(it);
        post_handler(std::move(t), ec);
        return res;
    }
};

const asio::execution_context::id mock_timer_service::id;

// A mock for asio::steady_timer
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
            // If the handler had an executor, use this. Otherwise, use the timer's
            auto ex = asio::get_associated_executor(h, self->ex_);
            self->svc_->add_timer({self->expiry_, ex, std::forward<Handler>(h), self->timer_id_});
        }
    };

public:
    mock_timer(asio::any_io_executor ex)
        : svc_(&asio::use_service<mock_timer_service>(ex.context())),
          timer_id_(svc_->allocate_timer_id()),
          ex_(std::move(ex)),
          expiry_(svc_->current_time())
    {
    }

    asio::any_io_executor get_executor() { return ex_; }

    std::size_t expires_at(steady_clock::time_point new_expiry)
    {
        // cancel anything in flight, then set expiry
        std::size_t res = svc_->cancel(timer_id_);
        expiry_ = new_expiry;
        return res;
    }

    std::size_t expires_after(steady_clock::duration dur) { return expires_at(svc_->current_time() + dur); }

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

enum fn_type
{
    connect,
    reset,
    ping,
};

// A mock for mysql::any_connection. This allows us to control
// when and how operations like async_connect or async_ping complete,
// make assertions on the passed parameters, and force error conditions.
// By default, mocked operations stay outstanding until they're acknowledged
// by the test by calling mock_connection::step. step will wait until the appropriate
// mocked function is called and will make the outstanding operation complete with
// the passed error_code/diagnostics. All this synchronization uses channels.
class mock_connection
{
    asio::experimental::channel<void(error_code, fn_type)> to_test_chan_;
    asio::experimental::channel<void(error_code, diagnostics)> from_test_chan_;

    // Code shared between all mocked ops
    struct mocked_op
    {
        mock_connection& obj;
        fn_type op_type;
        diagnostics* diag;

        template <class Self>
        void operator()(Self& self)
        {
            // Notify the test that we're about to do op_type
            obj.to_test_chan_.async_send(error_code(), op_type, std::move(self));
        }

        template <class Self>
        void operator()(Self& self, error_code ec)
        {
            if (ec)
            {
                // We were cancelled
                self.complete(ec);
            }
            else
            {
                // Read from the test what we should return
                obj.from_test_chan_.async_receive(std::move(self));
            }
        }

        template <class Self>
        void operator()(Self& self, error_code ec, diagnostics recv_diag)
        {
            // Done
            if (diag)
                *diag = std::move(recv_diag);
            self.complete(ec);
        }
    };

    struct step_op
    {
        mock_connection& obj;
        fn_type expected_op_type;
        error_code op_ec;
        diagnostics op_diag;

        template <class Self>
        void operator()(Self& self)
        {
            // Wait until the code under test performs the operation we want
            obj.to_test_chan_.async_receive(std::move(self));
        }

        template <class Self>
        void operator()(Self& self, error_code ec, fn_type actual_op_type)
        {
            // Verify it was actually what we wanted
            BOOST_TEST(ec == error_code());
            BOOST_TEST(actual_op_type == expected_op_type);

            // Tell the operation what its result should be
            obj.from_test_chan_.async_send(op_ec, std::move(op_diag), std::move(self));
        }

        template <class Self>
        void operator()(Self& self, error_code ec)
        {
            // Done
            BOOST_TEST(ec == error_code());
            self.complete();
        }
    };

    template <class CompletionToken>
    auto op_impl(fn_type op_type, diagnostics* diag, CompletionToken&& token)
        -> decltype(asio::async_compose<CompletionToken, void(error_code)>(
            std::declval<mocked_op>(),
            token,
            asio::any_io_executor()
        ))
    {
        return asio::async_compose<CompletionToken, void(error_code)>(
            mocked_op{*this, op_type, diag},
            token,
            to_test_chan_.get_executor()
        );
    }

public:
    boost::mysql::any_connection_params ctor_params;
    boost::mysql::connect_params last_connect_params;

    mock_connection(asio::any_io_executor ex, boost::mysql::any_connection_params ctor_params)
        : to_test_chan_(ex), from_test_chan_(std::move(ex)), ctor_params(ctor_params)
    {
    }

    template <class CompletionToken>
    auto async_connect(const connect_params* params, diagnostics& diag, CompletionToken&& token)
        -> decltype(op_impl(fn_type::connect, &diag, std::forward<CompletionToken>(token)))
    {
        BOOST_TEST(params != nullptr);
        last_connect_params = *params;
        return op_impl(fn_type::connect, &diag, std::forward<CompletionToken>(token));
    }

    template <class CompletionToken>
    auto async_ping(CompletionToken&& token)
        -> decltype(op_impl(fn_type::ping, nullptr, std::forward<CompletionToken>(token)))
    {
        return op_impl(fn_type::ping, nullptr, std::forward<CompletionToken>(token));
    }

    template <class CompletionToken>
    auto async_reset_connection(CompletionToken&& token)
        -> decltype(op_impl(fn_type::reset, nullptr, std::forward<CompletionToken>(token)))
    {
        return op_impl(fn_type::reset, nullptr, std::forward<CompletionToken>(token));
    }

    void step(
        fn_type expected_op_type,
        asio::any_completion_handler<void()> handler,
        error_code ec = {},
        diagnostics diag = {}
    )
    {
        return asio::async_compose<asio::any_completion_handler<void()>, void()>(
            step_op{*this, expected_op_type, ec, std::move(diag)},
            handler,
            from_test_chan_
        );
    }
};

// Mock for io_traits
struct mock_io_traits
{
    using connection_type = mock_connection;
    using timer_type = mock_timer;
};

struct mock_pooled_connection;
using mock_node = basic_connection_node<mock_io_traits>;
using mock_pool = basic_pool_impl<mock_io_traits, mock_pooled_connection>;

// Mock for pooled_connection
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

// Helper class to launch an async_get_connection and then wait for it
// and validate its results
class get_connection_task
{
    struct impl_t
    {
        asio::steady_timer tim;
        mock_pool& pool;
        executor_info exec_info;
        mock_node* actual_node{};
        mock_pool* actual_pool{};
        error_code actual_ec;

        impl_t(mock_pool& p) : tim(p.get_executor(), steady_clock::time_point::max()), pool(p), exec_info{} {}
    };

    std::shared_ptr<impl_t> impl_;

    void wait_impl(
        mock_node* expected_node,
        error_code expected_ec,
        asio::any_completion_handler<void()> handler
    )
    {
        struct intermediate_handler
        {
            std::shared_ptr<impl_t> impl;
            mock_node* expected_node;
            error_code expected_ec;
            asio::any_completion_handler<void()> final_handler;

            using executor_type = asio::any_io_executor;
            executor_type get_executor() const { return impl->pool.get_executor(); }

            void operator()(error_code)
            {
                auto* expected_pool = expected_ec ? nullptr : &impl->pool;
                BOOST_TEST(impl->actual_ec == expected_ec);
                BOOST_TEST(impl->actual_pool == expected_pool);
                BOOST_TEST(impl->actual_node == expected_node);
                std::move(final_handler)();
            }
        };

        impl_->tim.async_wait(intermediate_handler{impl_, expected_node, expected_ec, std::move(handler)});
    }

public:
    get_connection_task() = default;

    get_connection_task(
        mock_pool& pool,
        std::chrono::steady_clock::duration timeout,
        diagnostics* diag = nullptr
    )
        : impl_(std::make_shared<impl_t>(pool))
    {
        auto ex = create_tracker_executor(pool.get_executor(), &impl_->exec_info);
        auto impl = impl_;
        pool.async_get_connection(
            asio::use_service<mock_timer_service>(ex.context()).current_time() + timeout,
            diag,
            asio::bind_executor(
                ex,
                [ex, impl](error_code ec, mock_pooled_connection c) {
                    BOOST_TEST(impl->exec_info.total() > 0u);
                    impl->actual_node = c.node;
                    impl->actual_pool = c.pool.get();
                    impl->actual_ec = ec;
                    impl->tim.expires_at(steady_clock::time_point::min());
                }
            )
        );
    }

    void wait(mock_node& expected_node, asio::any_completion_handler<void()> handler)
    {
        wait_impl(&expected_node, error_code(), std::move(handler));
    }

    void wait(error_code expected_ec, asio::any_completion_handler<void()> handler)
    {
        wait_impl(nullptr, expected_ec, std::move(handler));
    }
};

// The base class for all test ops. All test must define an op struct derived
// from pool_test_op, defining an invoke() coroutine running the test,
// then call pool_test<op>
class pool_test_op_base : public asio::coroutine
{
public:
    pool_test_op_base(mock_pool& pool, bool& finished) : pool_(&pool), finished_(&finished) {}

    virtual ~pool_test_op_base() {}
    virtual void launch() = 0;

    using executor_type = asio::any_io_executor;
    asio::any_io_executor get_executor() const { return pool_->get_executor(); }

protected:
    mock_pool* pool_{};
    bool* finished_{};
    bool initial_{true};

    void poll() { static_cast<asio::io_context&>(pool_->get_executor().context()).poll(); }

    void check_shared_st(
        error_code expected_ec,
        const diagnostics& expected_diag,
        std::size_t expected_num_pending,
        std::size_t expected_num_idle
    )
    {
        const auto& st = pool_->shared_state();
        BOOST_TEST(st.last_ec == expected_ec);
        BOOST_TEST(st.last_diag == expected_diag);
        BOOST_TEST(st.num_pending_connections == expected_num_pending);
        BOOST_TEST(st.idle_list.size() == expected_num_idle);
    }

    mock_timer_service& get_timer_service()
    {
        return asio::use_service<mock_timer_service>(pool_->get_executor().context());
    }

    mock_pool& get_pool() noexcept { return *pool_; }

    // Wrapper for waiting for a status on a certain node
    void wait_for_status(mock_node& node, connection_status status)
    {
        poll();
        BOOST_TEST(node.status() == status);
    }

    // Waits until the number of pending requests in the pool equals a certain number
    void wait_for_num_requests(std::size_t num_requests)
    {
        poll();
        BOOST_TEST(pool_->num_pending_requests() == num_requests);
    }

    // Waits until there is at least num_nodes connections in the list
    void wait_for_num_nodes(std::size_t num_nodes)
    {
        poll();
        BOOST_TEST(pool_->nodes().size() == num_nodes);
    }
};

template <class D>
class pool_test_op : public pool_test_op_base
{
    D& derived_this() { return static_cast<D&>(*this); }

public:
    using pool_test_op_base::pool_test_op_base;

    // Wrapper for calling mock_connection::step()
    void step(mock_node& node, fn_type next_act, error_code ec = {}, diagnostics diag = {})
    {
        node.connection().step(next_act, std::move(derived_this()), ec, diag);
    }

    // Wrapper for get_connection_task::wait(). It helps prevent lifetime
    // issues with std::move(*this)
    void wait_for_task(get_connection_task task, mock_node& expected_node)
    {
        task.wait(expected_node, std::move(derived_this()));
    }

    void wait_for_task(get_connection_task task, error_code expected_ec)
    {
        task.wait(expected_ec, std::move(derived_this()));
    }

    void launch() final { (*this)(); }

    virtual std::size_t initial_num_nodes() const { return 1u; }

    void operator()()
    {
        if (initial_)
        {
            initial_ = false;
            wait_for_num_nodes(initial_num_nodes());
        }
        derived_this().invoke();
        if (derived_this().is_complete())
        {
            pool_->cancel();
            *finished_ = true;
        }
    }
};

// The test body
static void pool_test_impl(
    boost::mysql::pool_params params,
    std::function<std::unique_ptr<pool_test_op_base>(mock_pool&, bool&)> test_fun
)
{
    // I/O context
    asio::io_context ctx;

    // Pool (must be created using dynamic memory)
    auto pool = std::make_shared<mock_pool>(ctx, std::move(params));

    // This flag is only set to true after the test finishes.
    // If the test timeouts, it will be false
    bool finished = false;

    // Run the pool
    pool->async_run([](error_code ec) { BOOST_TEST(ec == error_code()); });

    // Launch the test
    auto test = test_fun(*pool, finished);
    test->launch();

    // If the test doesn't complete in this time, there was an error
    ctx.run_for(std::chrono::seconds(100));

    // Check that we didn't timeout
    BOOST_TEST(finished == true);
}

template <class TestOp, class... Args>
static void pool_test(boost::mysql::pool_params params, Args&&... args)
{
    pool_test_impl(std::move(params), [&](mock_pool& pool, bool& finished) mutable {
        return std::unique_ptr<pool_test_op_base>{new TestOp(pool, finished, std::forward<Args>(args)...)};
    });
}

// connection lifecycle
BOOST_AUTO_TEST_CASE(lifecycle_connect_error)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        static diagnostics expected_diag() { return create_server_diag("Connection error!"); }

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Connection trying to connect
                wait_for_status(node, connection_status::connect_in_progress);
                check_shared_st(error_code(), diagnostics(), 1, 0);

                // Connect fails, so the connection goes to sleep. Diagnostics are stored in shared state.
                BOOST_ASIO_CORO_YIELD
                step(node, fn_type::connect, common_server_errc::er_aborting_connection, expected_diag());
                wait_for_status(node, connection_status::sleep_connect_failed_in_progress);
                check_shared_st(common_server_errc::er_aborting_connection, expected_diag(), 1, 0);

                // Advance until it's time to retry again
                get_timer_service().advance_time_by(std::chrono::seconds(2));
                wait_for_status(node, connection_status::connect_in_progress);
                check_shared_st(common_server_errc::er_aborting_connection, expected_diag(), 1, 0);

                // Connection connects successfully this time. Diagnostics have
                // been cleared and the connection is marked as idle
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_params params;
    params.retry_interval = std::chrono::seconds(2);

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(lifecycle_connect_timeout)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Connection trying to connect
                wait_for_status(node, connection_status::connect_in_progress);

                // Timeout ellapses. Connect is considered failed
                get_timer_service().advance_time_by(std::chrono::seconds(5));
                wait_for_status(node, connection_status::sleep_connect_failed_in_progress);
                check_shared_st(client_errc::timeout, diagnostics(), 1, 0);

                // Advance until it's time to retry again
                get_timer_service().advance_time_by(std::chrono::seconds(2));
                wait_for_status(node, connection_status::connect_in_progress);
                check_shared_st(client_errc::timeout, diagnostics(), 1, 0);

                // Connection connects successfully this time
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_params params;
    params.connect_timeout = std::chrono::seconds(5);
    params.retry_interval = std::chrono::seconds(2);

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(lifecycle_return_without_reset)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Wait until a connection is successfully connected
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);

                // Simulate a user picking the connection
                node.mark_as_in_use();
                check_shared_st(error_code(), diagnostics(), 0, 0);

                // Simulate a user returning the connection (without reset)
                node.mark_as_collectable(false);

                // The connection goes back to idle without invoking resets
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_test<op>(pool_params{});
}

BOOST_AUTO_TEST_CASE(lifecycle_reset_success)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Wait until a connection is successfully connected, then pick it up
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                node.mark_as_in_use();

                // Simulate a user returning the connection (with reset)
                node.mark_as_collectable(true);

                // A reset is issued
                wait_for_status(node, connection_status::reset_in_progress);
                check_shared_st(error_code(), diagnostics(), 1, 0);

                // Successful reset makes the connection idle again
                BOOST_ASIO_CORO_YIELD step(node, fn_type::reset);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_test<op>(pool_params{});
}

BOOST_AUTO_TEST_CASE(lifecycle_reset_error)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Connect, pick up and return a connection
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                node.mark_as_in_use();
                node.mark_as_collectable(true);
                wait_for_status(node, connection_status::reset_in_progress);

                // Reset fails. This triggers a reconnection. Diagnostics are not saved
                BOOST_ASIO_CORO_YIELD
                step(node, fn_type::reset, common_server_errc::er_aborting_connection);
                wait_for_status(node, connection_status::connect_in_progress);
                check_shared_st(error_code(), diagnostics(), 1, 0);

                // Reconnect succeeds. We're idle again
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_test<op>(pool_params{});
}

BOOST_AUTO_TEST_CASE(lifecycle_reset_timeout)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Connect, pick up and return a connection
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                node.mark_as_in_use();
                node.mark_as_collectable(true);
                wait_for_status(node, connection_status::reset_in_progress);

                // Reset times out. This triggers a reconnection
                get_timer_service().advance_time_by(std::chrono::seconds(1));
                wait_for_status(node, connection_status::connect_in_progress);
                check_shared_st(error_code(), diagnostics(), 1, 0);

                // Reconnect succeeds. We're idle again
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_params params;
    params.ping_timeout = std::chrono::seconds(1);

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(lifecycle_reset_timeout_disabled)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Connect, pick up and return a connection
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                node.mark_as_in_use();
                node.mark_as_collectable(true);
                wait_for_status(node, connection_status::reset_in_progress);

                // Reset doesn't time out, regardless of how much time we wait
                get_timer_service().advance_time_by(std::chrono::hours(9999));
                BOOST_ASIO_CORO_YIELD asio::post(std::move(*this));
                BOOST_TEST(node.status() == connection_status::reset_in_progress);
                check_shared_st(error_code(), diagnostics(), 1, 0);

                // Reset succeeds
                BOOST_ASIO_CORO_YIELD step(node, fn_type::reset);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_params params;
    params.ping_timeout = std::chrono::seconds(0);

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(lifecycle_ping_success)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Wait until a connection is successfully connected
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);

                // Wait until ping interval ellapses. This triggers a ping
                get_timer_service().advance_time_by(std::chrono::seconds(100));
                wait_for_status(node, connection_status::ping_in_progress);
                check_shared_st(error_code(), diagnostics(), 1, 0);

                // After ping succeeds, connection goes back to idle
                BOOST_ASIO_CORO_YIELD step(node, fn_type::ping);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_params params;
    params.ping_interval = std::chrono::seconds(100);

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(lifecycle_ping_error)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Wait until a connection is successfully connected
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);

                // Wait until ping interval ellapses
                get_timer_service().advance_time_by(std::chrono::seconds(100));

                // Ping fails. This triggers a reconnection. Diagnostics are not saved
                BOOST_ASIO_CORO_YIELD
                step(node, fn_type::ping, common_server_errc::er_aborting_connection);
                wait_for_status(node, connection_status::connect_in_progress);
                check_shared_st(error_code(), diagnostics(), 1, 0);

                // Reconnection succeeds
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_params params;
    params.ping_interval = std::chrono::seconds(100);

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(lifecycle_ping_timeout)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Wait until a connection is successfully connected
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);

                // Wait until ping interval ellapses
                get_timer_service().advance_time_by(std::chrono::seconds(100));
                wait_for_status(node, connection_status::ping_in_progress);

                // Ping times out. This triggers a reconnection. Diagnostics are not saved
                get_timer_service().advance_time_by(std::chrono::seconds(2));
                wait_for_status(node, connection_status::connect_in_progress);

                // Reconnection succeeds
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_params params;
    params.ping_interval = std::chrono::seconds(100);
    params.ping_timeout = std::chrono::seconds(2);

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(lifecycle_ping_timeout_disabled)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Wait until a connection is successfully connected
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);

                // Wait until ping interval ellapses
                get_timer_service().advance_time_by(std::chrono::seconds(100));
                wait_for_status(node, connection_status::ping_in_progress);

                // Ping doesn't time out, regardless of how much we wait
                get_timer_service().advance_time_by(std::chrono::hours(9999));
                BOOST_ASIO_CORO_YIELD asio::post(std::move(*this));
                BOOST_TEST(node.status() == connection_status::ping_in_progress);

                // Ping succeeds
                BOOST_ASIO_CORO_YIELD step(node, fn_type::ping);
                wait_for_status(node, connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_params params;
    params.ping_interval = std::chrono::seconds(100);
    params.ping_timeout = std::chrono::seconds(0);

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(lifecycle_ping_disabled)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Wait until a connection is successfully connected
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);

                // Connection won't ping, regardless of how much time we wait
                get_timer_service().advance_time_by(std::chrono::hours(9999));
                BOOST_ASIO_CORO_YIELD asio::post(std::move(*this));
                BOOST_TEST(node.status() == connection_status::idle);
                check_shared_st(error_code(), diagnostics(), 0, 1);
            }
        }
    };

    pool_params params;
    params.ping_interval = std::chrono::seconds(0);

    pool_test<op>(std::move(params));
}

// async_get_connection
BOOST_AUTO_TEST_CASE(get_connection_wait_success)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;
        get_connection_task task;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Connection tries to connect and fails
                BOOST_ASIO_CORO_YIELD
                step(node, fn_type::connect, common_server_errc::er_aborting_connection);
                wait_for_status(node, connection_status::sleep_connect_failed_in_progress);

                // A request for a connection is issued. The request doesn't find
                // any available connection, and the current one is pending, so no new connections are created
                task = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);
                wait_for_num_requests(1);
                BOOST_TEST(pool_->nodes().size() == 1u);

                // Retry interval ellapses and connection retries and succeeds
                get_timer_service().advance_time_by(std::chrono::seconds(2));
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);

                // Request is fulfilled
                BOOST_ASIO_CORO_YIELD wait_for_task(task, node);
                BOOST_TEST(node.status() == connection_status::in_use);
                BOOST_TEST(pool_->nodes().size() == 1u);
                BOOST_TEST(pool_->num_pending_requests() == 0u);
            }
        }
    };

    pool_params params;
    params.retry_interval = std::chrono::seconds(2);

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(get_connection_wait_timeout_no_diag)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;
        get_connection_task task;
        std::unique_ptr<diagnostics> diag{new diagnostics()};

        void invoke()
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // A request for a connection is issued. The request doesn't find
                // any available connection, and the current one is pending, so no new connections are created
                task = get_connection_task(*pool_, std::chrono::seconds(1), diag.get());
                wait_for_num_requests(1);
                BOOST_TEST(pool_->nodes().size() == 1u);

                // The request timeout ellapses, so the request fails
                get_timer_service().advance_time_by(std::chrono::seconds(1));
                BOOST_ASIO_CORO_YIELD wait_for_task(task, client_errc::timeout);
                BOOST_TEST(*diag == diagnostics());
                BOOST_TEST(pool_->nodes().size() == 1u);
                BOOST_TEST(pool_->num_pending_requests() == 0u);
            }
        }
    };

    pool_test<op>(pool_params{});
}

BOOST_AUTO_TEST_CASE(get_connection_wait_timeout_with_diag)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;
        get_connection_task task;
        std::unique_ptr<diagnostics> diag{new diagnostics()};

        void invoke()
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // A request for a connection is issued. The request doesn't find
                // any available connection, and the current one is pending, so no new connections are created
                task = get_connection_task(*pool_, std::chrono::seconds(1), diag.get());
                wait_for_num_requests(1);
                BOOST_TEST(pool_->nodes().size() == 1u);

                // The connection fails to connect
                BOOST_ASIO_CORO_YIELD
                step(
                    *pool_->nodes().begin(),
                    fn_type::connect,
                    common_server_errc::er_bad_db_error,
                    create_server_diag("Bad db")
                );

                // The request timeout ellapses, so the request fails
                get_timer_service().advance_time_by(std::chrono::seconds(1));
                BOOST_ASIO_CORO_YIELD wait_for_task(task, common_server_errc::er_bad_db_error);
                BOOST_TEST(*diag == create_server_diag("Bad db"));
                BOOST_TEST(pool_->nodes().size() == 1u);
                BOOST_TEST(pool_->num_pending_requests() == 0u);
            }
        }
    };

    pool_test<op>(pool_params{});
}

BOOST_AUTO_TEST_CASE(get_connection_wait_timeout_with_diag_nullptr)
{
    // We don't crash if diag is nullptr

    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;
        get_connection_task task;

        void invoke()
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // A request for a connection is issued. The request doesn't find
                // any available connection, and the current one is pending, so no new connections are created
                task = get_connection_task(*pool_, std::chrono::seconds(1), nullptr);
                wait_for_num_requests(1);
                BOOST_TEST(pool_->nodes().size() == 1u);

                // The connection fails to connect
                BOOST_ASIO_CORO_YIELD
                step(
                    *pool_->nodes().begin(),
                    fn_type::connect,
                    common_server_errc::er_bad_db_error,
                    create_server_diag("Bad db")
                );

                // The request timeout ellapses, so the request fails
                get_timer_service().advance_time_by(std::chrono::seconds(1));
                BOOST_ASIO_CORO_YIELD wait_for_task(task, common_server_errc::er_bad_db_error);
                BOOST_TEST(pool_->nodes().size() == 1u);
                BOOST_TEST(pool_->num_pending_requests() == 0u);
            }
        }
    };

    pool_test<op>(pool_params{});
}

BOOST_AUTO_TEST_CASE(get_connection_immediate_completion)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Wait for a connection to be ready
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);
                wait_for_status(node, connection_status::idle);

                // A request for a connection is issued. The request completes immediately
                BOOST_ASIO_CORO_YIELD
                wait_for_task(get_connection_task(*pool_, std::chrono::seconds(5), nullptr), node);
                BOOST_TEST(node.status() == connection_status::in_use);
                BOOST_TEST(pool_->nodes().size() == 1u);
                BOOST_TEST(pool_->num_pending_requests() == 0u);
            }
        }
    };

    pool_test<op>(pool_params{});
}

BOOST_AUTO_TEST_CASE(get_connection_connection_creation)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;
        mock_node* node2{};
        get_connection_task task2, task3;

        void invoke()
        {
            auto& node1 = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Wait for a connection to be ready, then get it from the pool
                BOOST_ASIO_CORO_YIELD step(node1, fn_type::connect);
                wait_for_status(node1, connection_status::idle);
                BOOST_ASIO_CORO_YIELD
                wait_for_task(get_connection_task(*pool_, std::chrono::seconds(5), nullptr), node1);

                // Another request is issued. The connection we have is in use, so another one is created.
                // Since this is not immediate, the task will need to wait
                task2 = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);
                wait_for_num_requests(1);
                node2 = &*std::next(pool_->nodes().begin());

                // Connection connects successfully and is handed to us
                BOOST_ASIO_CORO_YIELD step(*node2, fn_type::connect);
                BOOST_ASIO_CORO_YIELD wait_for_task(task2, *node2);
                BOOST_TEST(node2->status() == connection_status::in_use);
                BOOST_TEST(pool_->nodes().size() == 2u);
                BOOST_TEST(pool_->num_pending_requests() == 0u);

                // Another request is issued. All connections are in use but max size is already
                // reached, so no new connection is created
                task3 = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);
                wait_for_num_requests(1);
                BOOST_TEST(pool_->nodes().size() == 2u);

                // When one of the connections is returned, the request is fulfilled
                node2->mark_as_collectable(false);
                BOOST_ASIO_CORO_YIELD wait_for_task(task3, *node2);
                BOOST_TEST(pool_->num_pending_requests() == 0u);
                BOOST_TEST(pool_->nodes().size() == 2u);
            }
        }
    };

    pool_params params;
    params.initial_size = 1;
    params.max_size = 2;

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(get_connection_multiple_requests)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;
        mock_node *node1{}, *node2{};
        get_connection_task task1, task2, task3, task4, task5;

        // 2 connections are created from the beginning
        std::size_t initial_num_nodes() const final { return 2u; }

        void invoke()
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Issue some parallel requests
                task1 = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);
                task2 = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);
                task3 = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);
                task4 = get_connection_task(*pool_, std::chrono::seconds(2), nullptr);
                task5 = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);

                // Two connections can be created. These fulfill two requests
                node1 = &pool_->nodes().front();
                node2 = &*std::next(pool_->nodes().begin());
                BOOST_ASIO_CORO_YIELD step(*node1, fn_type::connect);
                BOOST_ASIO_CORO_YIELD step(*node2, fn_type::connect);
                BOOST_ASIO_CORO_YIELD wait_for_task(task1, *node1);
                BOOST_ASIO_CORO_YIELD wait_for_task(task2, *node2);

                // Time ellapses and task4 times out
                get_timer_service().advance_time_by(std::chrono::seconds(2));
                BOOST_ASIO_CORO_YIELD wait_for_task(task4, client_errc::timeout);

                // A connection is returned. The first task to enter is served
                node1->mark_as_collectable(true);
                BOOST_ASIO_CORO_YIELD step(*node1, fn_type::reset);
                BOOST_ASIO_CORO_YIELD wait_for_task(task3, *node1);

                // The next connection to be returned is for task5
                node2->mark_as_collectable(false);
                BOOST_ASIO_CORO_YIELD wait_for_task(task5, *node2);

                // Done
                BOOST_TEST(pool_->num_pending_requests() == 0u);
                BOOST_TEST(pool_->nodes().size() == 2u);
            }
        }
    };

    pool_params params;
    params.initial_size = 2;
    params.max_size = 2;

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(get_connection_cancel)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;
        mock_node *node1{}, *node2{};
        get_connection_task task1, task2;

        void invoke()
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Issue some requests
                task1 = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);
                task2 = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);
                wait_for_num_requests(2);

                // While in flight, cancel the pool
                pool_->cancel();

                // All tasks fail with a cancelled code
                BOOST_ASIO_CORO_YIELD wait_for_task(task1, client_errc::cancelled);
                BOOST_ASIO_CORO_YIELD wait_for_task(task2, client_errc::cancelled);

                // Further tasks fail immediately
                BOOST_ASIO_CORO_YIELD wait_for_task(
                    get_connection_task(*pool_, std::chrono::seconds(5), nullptr),
                    client_errc::cancelled
                );
            }
        }
    };

    pool_test<op>(pool_params{});
}

// pool size 0 works
BOOST_AUTO_TEST_CASE(get_connection_initial_size_0)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;
        get_connection_task task;

        std::size_t initial_num_nodes() const final { return 0u; }

        void invoke()
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                // No connections created at this point. A connection request arrives
                BOOST_TEST(pool_->nodes().size() == 0u);
                task = get_connection_task(*pool_, std::chrono::seconds(5), nullptr);

                // This creates a new connection, which fulfills the request
                wait_for_num_requests(1);
                BOOST_TEST(pool_->nodes().size() == 1u);
                BOOST_ASIO_CORO_YIELD step(pool_->nodes().front(), fn_type::connect);
                BOOST_ASIO_CORO_YIELD wait_for_task(task, pool_->nodes().front());
            }
        }
    };

    pool_params params;
    params.initial_size = 0;

    pool_test<op>(std::move(params));
}

// pool_params have the intended effect
BOOST_AUTO_TEST_CASE(params_ssl_ctx_buffsize)
{
    struct op : pool_test_op<op>
    {
        asio::ssl::context::native_handle_type expected_handle;

        op(mock_pool& pool, bool& finished, asio::ssl::context::native_handle_type ssl_handle)
            : pool_test_op<op>(pool, finished), expected_handle(ssl_handle)
        {
        }

        void invoke()
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                auto ctor_params = pool_->nodes().front().connection().ctor_params;
                BOOST_TEST_REQUIRE(ctor_params.ssl_context != nullptr);
                BOOST_TEST(ctor_params.ssl_context->native_handle() == expected_handle);
                BOOST_TEST(ctor_params.initial_read_buffer_size == 16u);
            }
        }
    };

    // Pass a custom ssl context and buffer size
    pool_params params;
    params.ssl_ctx.emplace(boost::asio::ssl::context::tlsv12_client);
    params.initial_read_buffer_size = 16u;

    // SSL context matching is performed using the underlying handle
    // because ssl::context provides no way to query the options previously set
    auto handle = params.ssl_ctx->native_handle();

    pool_test<op>(std::move(params), handle);
}

BOOST_AUTO_TEST_CASE(params_connect_1)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Connect
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);

                // Check params
                const auto& cparams = node.connection().last_connect_params;
                BOOST_TEST(cparams.connection_collation == std::uint16_t(0));
                BOOST_TEST(cparams.server_address.hostname() == "myhost");
                BOOST_TEST(cparams.server_address.port() == std::uint16_t(1234));
                BOOST_TEST(cparams.username == "myuser");
                BOOST_TEST(cparams.password == "mypasswd");
                BOOST_TEST(cparams.database == "mydb");
                BOOST_TEST(cparams.ssl == boost::mysql::ssl_mode::disable);
                BOOST_TEST(cparams.multi_queries == true);
            }
        }
    };

    pool_params params;
    params.server_address.emplace_host_and_port("myhost", 1234);
    params.username = "myuser";
    params.password = "mypasswd";
    params.database = "mydb";
    params.ssl = boost::mysql::ssl_mode::disable;
    params.multi_queries = true;

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_CASE(params_connect_2)
{
    struct op : pool_test_op<op>
    {
        using pool_test_op<op>::pool_test_op;

        void invoke()
        {
            auto& node = pool_->nodes().front();

            BOOST_ASIO_CORO_REENTER(*this)
            {
                // Connect
                BOOST_ASIO_CORO_YIELD step(node, fn_type::connect);

                // Check params
                const auto& cparams = node.connection().last_connect_params;
                BOOST_TEST(cparams.connection_collation == std::uint16_t(0));
                BOOST_TEST(cparams.server_address.unix_socket_path() == "/mysock");
                BOOST_TEST(cparams.username == "myuser2");
                BOOST_TEST(cparams.password == "mypasswd2");
                BOOST_TEST(cparams.database == "mydb2");
                BOOST_TEST(cparams.ssl == boost::mysql::ssl_mode::require);
                BOOST_TEST(cparams.multi_queries == false);
            }
        }
    };

    pool_params params;
    params.server_address.emplace_unix_path("/mysock");
    params.username = "myuser2";
    params.password = "mypasswd2";
    params.database = "mydb2";
    params.ssl = boost::mysql::ssl_mode::require;
    params.multi_queries = false;

    pool_test<op>(std::move(params));
}

BOOST_AUTO_TEST_SUITE_END()
