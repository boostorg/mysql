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
#include <boost/mysql/pooled_connection.hpp>

#include <boost/mysql/detail/connection_pool/connection_node.hpp>
#include <boost/mysql/detail/connection_pool/connection_pool_impl.hpp>
#include <boost/mysql/detail/connection_pool/internal_pool_params.hpp>
#include <boost/mysql/detail/connection_pool/sansio_connection_node.hpp>

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
#include <stdexcept>

#include "pool_printing.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_common/run_stackful_coro.hpp"
#include "test_unit/run_coroutine.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
namespace asio = boost::asio;
using boost::mysql::common_server_errc;
using boost::mysql::connect_params;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
using boost::mysql::pooled_connection;
using std::chrono::steady_clock;
using timeout_t = std::chrono::steady_clock::duration;

/**
 * connection lifecycle
 *   connect error
 *   connect success
 *   idle wait results in ping, success
 *   idle wait results in ping, error & reconnection
 *   conn retrieved, returned without reset
 *   conn retrieved, returned with reset, success
 *   conn retrieved, returned with reset, error & reconnection
 */

BOOST_AUTO_TEST_SUITE(test_connection_node)

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

    void expires_at(steady_clock::time_point new_expiry)
    {
        svc_->cancel(timer_id_);
        expiry_ = new_expiry;
    }

    void expires_after(steady_clock::duration dur) { expires_at(svc_->current_time() + dur); }

    std::size_t cancel() { return svc_->cancel(timer_id_); }

    template <class CompletionToken>
    auto async_wait(CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(initiate_wait(), token, this))
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

    template <class CompletionToken>
    auto op_impl(next_connection_action act, diagnostics* diag, CompletionToken&& token)
    {
        return asio::async_compose<CompletionToken, void(error_code)>(
            run_op{*this, act, diag},
            token,
            recv_chan_.get_executor()
        );
    }

public:
    mock_connection(asio::any_io_executor ex, boost::mysql::any_connection_params)
        : recv_chan_(ex), send_chan_(std::move(ex))
    {
    }

    template <class CompletionToken>
    auto async_connect(const connect_params*, diagnostics& diag, CompletionToken&& token)
        -> decltype(op_impl(next_connection_action{}, &diag, std::forward<CompletionToken>(token)))
    {
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

    void wait_for_step(
        next_connection_action act,
        asio::yield_context yield,
        error_code ec = {},
        diagnostics diag = {}
    )
    {
        auto actual_act = recv_chan_.async_receive(yield);
        BOOST_TEST(actual_act == act);
        send_chan_.async_send(ec, std::move(diag), yield);
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

void rethrow_on_err(std::exception_ptr err)
{
    if (err)
        std::rethrow_exception(err);
}

void check_err(error_code ec) { BOOST_TEST(ec == error_code()); }

void post_until(std::function<bool()> cond, asio::yield_context yield)
{
    for (int i = 0; i < 10; ++i)
    {
        if (cond())
            return;
        asio::post(yield);
    }
    BOOST_TEST_REQUIRE(false);
}

BOOST_AUTO_TEST_CASE(connect_timeout)
{
    asio::io_context ctx;
    boost::mysql::pool_params params;
    params.retry_interval = std::chrono::seconds(2);
    auto internal_params = make_internal_pool_params(std::move(params));
    mock_shared_state st;

    mock_node node(internal_params, ctx.get_executor(), ctx.get_executor(), st);
    bool finished = false;

    asio::spawn(
        ctx,
        [&](asio::yield_context yield) {
            // Connection tries to connect and fails
            node.connection().wait_for_step(
                next_connection_action::connect,
                yield,
                common_server_errc::er_aborting_connection,
                create_server_diag("Connection error!")
            );

            // Wait until the connection is sleeping
            post_until(
                [&] { return node.status() == connection_status::sleep_connect_failed_in_progress; },
                yield
            );

            // Diagnostics are stored in shared state
            BOOST_TEST(st.last_ec == error_code(common_server_errc::er_aborting_connection));
            BOOST_TEST(st.last_diag == create_server_diag("Connection error!"));

            // Advance until it's time to retry again
            asio::use_service<mock_timer_service>(ctx).advance_time_by(std::chrono::seconds(2));

            // Connection connects successfully this time
            node.connection().wait_for_step(next_connection_action::connect, yield, error_code());
            post_until([&] { return node.status() == connection_status::idle; }, yield);

            // Diagnostics have been cleared
            BOOST_TEST(st.last_ec == error_code());
            BOOST_TEST(st.last_diag == diagnostics());

            // The connection is marked as idle
            BOOST_TEST(&st.idle_list.front() == &node);

            // Finish
            node.cancel();
            finished = true;
        },
        rethrow_on_err
    );

    node.async_run(check_err);

    ctx.run_for(std::chrono::seconds(10));
    BOOST_TEST(finished == true);
}

// connection_pool_impl

BOOST_AUTO_TEST_CASE(wait_success)
{
    asio::io_context ctx;
    boost::mysql::pool_params params;
    params.retry_interval = std::chrono::seconds(2);

    auto pool = std::make_shared<mock_pool>(ctx, std::move(params));

    bool finished = false;

    asio::spawn(
        ctx,
        [&](asio::yield_context yield) {
            // Wait for some connections to be created
            post_until([&] { return !pool->nodes().empty(); }, yield);
            auto& node = *pool->nodes().begin();
            auto& svc = asio::use_service<mock_timer_service>(ctx);

            // Connection tries to connect and fails
            node.connection().wait_for_step(
                next_connection_action::connect,
                yield,
                common_server_errc::er_aborting_connection
            );

            // Connection goes to sleep
            post_until(
                [&] { return node.status() == connection_status::sleep_connect_failed_in_progress; },
                yield
            );

            // A request for a connection is issued. The request doesn't find
            // any available connection, and the current one is pending, so no new connections are created
            asio::experimental::channel<void(error_code, mock_pooled_connection)> subtask_chan{
                yield.get_executor()
            };
            pool->async_get_connection(
                std::chrono::seconds(5),
                nullptr,
                [&](error_code ec, mock_pooled_connection c) {
                    subtask_chan.async_send(ec, std::move(c), asio::detached);
                }
            );
            post_until([&] { return pool->num_pending_requests() > 0u; }, yield);
            BOOST_TEST(pool->nodes().size() == 1u);

            // Retry interval ellapses and connection retries and succeeds
            svc.advance_time_by(std::chrono::seconds(2));
            node.connection().wait_for_step(next_connection_action::connect, yield);

            // Request is fulfilled
            auto pooled_conn = subtask_chan.async_receive(yield);
            BOOST_TEST(pooled_conn.node == &node);
            BOOST_TEST(node.status() == connection_status::in_use);
            BOOST_TEST(pool->nodes().size() == 1u);
            BOOST_TEST(pool->num_pending_requests() == 0u);

            // Finish
            pool->cancel();
            finished = true;
        },
        rethrow_on_err
    );

    pool->async_run(check_err);

    ctx.run_for(std::chrono::seconds(10));
    BOOST_TEST(finished == true);
}

BOOST_AUTO_TEST_CASE(wait_timeout_no_diag)
{
    asio::io_context ctx;
    boost::mysql::pool_params params;

    auto pool = std::make_shared<mock_pool>(ctx, std::move(params));

    bool finished = false;

    pool->async_run(check_err);

    asio::spawn(
        ctx,
        [&](asio::yield_context yield) {
            // A request for a connection is issued. The request doesn't find
            // any available connection, and the current one is pending, so no new connections are created
            diagnostics diag;
            asio::experimental::channel<void(error_code, mock_pooled_connection)> subtask_chan{
                yield.get_executor()
            };
            pool->async_get_connection(
                std::chrono::seconds(1),
                &diag,
                [&](error_code ec, mock_pooled_connection c) {
                    subtask_chan.async_send(ec, std::move(c), asio::detached);
                }
            );
            post_until([&] { return pool->num_pending_requests() > 0u; }, yield);
            BOOST_TEST(pool->nodes().size() == 1u);

            // The request timeout ellapses, so the request fails
            asio::use_service<mock_timer_service>(ctx).advance_time_by(std::chrono::seconds(1));
            error_code ec;
            auto pooled_conn = subtask_chan.async_receive(yield[ec]);
            BOOST_TEST(pooled_conn.node == nullptr);
            BOOST_TEST(pooled_conn.pool == nullptr);
            BOOST_TEST(ec == boost::mysql::client_errc::timeout);
            BOOST_TEST(diag == diagnostics());
            BOOST_TEST(pool->nodes().size() == 1u);

            // Finish
            pool->cancel();
            finished = true;
        },
        rethrow_on_err
    );

    ctx.run_for(std::chrono::seconds(100));
    BOOST_TEST(finished == true);
}

/**
 * get_connection
 *   not running
 *   terminated
 *   immediate
 *   with wait and retry (?)
 *   with wait timeout no diag
 *   with wait timeout diag
 *   no conn available, room for conns but some pending
 *   no conn available, room for conns
 *   no conn available, no room for conns
 *   the correct executor is used (token with executor)
 *   the correct executor is used (token without executor)
 *   the correct executor is used (immediate completion)
 *   connections and pool created with the adequate executor (maybe integ?)
 *   diag nullptr doesn't crash
 */

BOOST_AUTO_TEST_SUITE_END()
