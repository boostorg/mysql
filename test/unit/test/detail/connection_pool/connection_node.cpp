// h

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>

#include <boost/mysql/detail/connection_pool/connection_node.hpp>
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
#include <boost/asio/coroutine.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <functional>
#include <list>

#include "pool_printing.hpp"
#include "test_common/create_diagnostics.hpp"

using namespace boost::mysql::detail;
namespace asio = boost::asio;
using boost::mysql::connect_params;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
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
            auto slot = asio::get_associated_cancellation_slot(t.handler);
            pending_.push_front(std::move(t));
            if (slot.is_connected())
            {
                slot.emplace<cancel_handler>(cancel_handler{this, pending_.begin()});
            }
        }
    }

    void cancel(int timer_id)
    {
        for (auto it = pending_.begin(); it != pending_.end(); ++it)
        {
            if (it->timer_id == timer_id)
                fire_timer(it, asio::error::operation_aborted);
        }
    }

    void advance_time(steady_clock::time_point new_time)
    {
        for (auto it = pending_.begin(); it != pending_.end(); ++it)
        {
            if (it->expiry <= new_time)
                fire_timer(it, error_code());
        }
        current_time_ = new_time;
    }

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

    void fire_timer(std::list<pending_timer>::iterator it, error_code ec)
    {
        auto t = std::move(*it);
        pending_.erase(it);
        call_handler(std::move(t), ec);
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

    void cancel() { svc_->cancel(timer_id_); }

    template <class CompletionToken>
    auto async_wait(CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(initiate_wait(), token, this))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(initiate_wait(), token, this);
    }
};

class mock_connection
{
    using validator_fn = std::function<error_code(next_connection_action, diagnostics*)>;

    // asio::experimental::channel<void(error_code, next_connection_action)> recv_chan_;
    // asio::experimental::channel<void(error_code, diagnostics)> send_chan_;

    asio::any_io_executor ex_;
    asio::steady_timer tim_;
    validator_fn fn_{[](next_connection_action, diagnostics*) { return error_code(); }};

    struct initiation
    {
        template <class Handler>
        void operator()(Handler&& h, asio::steady_timer* tim, error_code ec)
        {
            tim->async_wait(asio::deferred([ec](error_code ec2) {
                return asio::deferred.values(ec2 ? ec2 : ec);
            }))(std::move(h));
        }
    };

    template <class CompletionToken>
    auto op_impl(next_connection_action act, diagnostics* diag, CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiation{},
            token,
            &tim_,
            error_code()
        ))
    {
        error_code ec = fn_(act, diag);
        tim_.expires_at(
            ec == boost::mysql::client_errc::cancelled ? steady_clock::time_point::max()
                                                       : steady_clock::time_point()
        );

        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiation{},
            token,
            &tim_,
            error_code(ec)
        );
    }

public:
    mock_connection(asio::any_io_executor ex, boost::mysql::any_connection_params)
        : ex_(std::move(ex)), tim_(ex_, steady_clock::time_point::max())
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

    void set_validator(validator_fn fn) { fn_ = std::move(fn); }
};

BOOST_AUTO_TEST_CASE(connect_timeout)
{
    asio::io_context ctx;
    boost::mysql::pool_params params;
    auto internal_params = make_internal_pool_params(std::move(params));
    conn_shared_state st(ctx.get_executor());
    basic_connection_node<mock_connection, mock_timer>
        node(internal_params, ctx.get_executor(), ctx.get_executor(), st);

    struct validator_t : asio::coroutine
    {
        error_code operator()(next_connection_action act, diagnostics* diag)
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                BOOST_TEST(act == next_connection_action::connect);
                *diag = boost::mysql::test::create_server_diag("Connect failed!");
                BOOST_ASIO_CORO_YIELD return boost::mysql::common_server_errc::er_bad_db_error;
                BOOST_TEST(act == next_connection_action::connect);
                diag->clear();
                return error_code();
            }
            assert(false);
            return error_code();
        }
    };

    node.connection().set_validator(validator_t());

    st.idle_notification_chan.async_receive([&node](error_code ec) {
        BOOST_TEST(ec == error_code());
        node.cancel();
    });

    auto fut = node.async_run(asio::use_future);

    ctx.run();
    fut.get();

    // TODO: check actual state
}

BOOST_AUTO_TEST_SUITE_END()
