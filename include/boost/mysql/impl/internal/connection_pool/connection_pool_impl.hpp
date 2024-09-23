//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_CONNECTION_POOL_IMPL_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_CONNECTION_POOL_IMPL_HPP

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>

#include <boost/mysql/impl/internal/connection_pool/connection_node.hpp>
#include <boost/mysql/impl/internal/connection_pool/internal_pool_params.hpp>
#include <boost/mysql/impl/internal/connection_pool/timer_list.hpp>
#include <boost/mysql/impl/internal/connection_pool/wait_group.hpp>
#include <boost/mysql/impl/internal/coroutine.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/core/ignore_unused.hpp>

#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

inline pipeline_request make_reset_pipeline()
{
    pipeline_request req;
    req.add_reset_connection().add_set_character_set(utf8mb4_charset);
    return req;
}

// Templating on ConnectionWrapper is useful for mocking in tests.
// Production code always uses ConnectionWrapper = pooled_connection.
template <class ConnectionType, class ClockType, class ConnectionWrapper>
class basic_pool_impl
    : public std::enable_shared_from_this<basic_pool_impl<ConnectionType, ClockType, ConnectionWrapper>>
{
    using this_type = basic_pool_impl<ConnectionType, ClockType, ConnectionWrapper>;
    using node_type = basic_connection_node<ConnectionType, ClockType>;
    using timer_type = asio::basic_waitable_timer<ClockType>;
    using shared_state_type = conn_shared_state<ConnectionType, ClockType>;

    enum class state_t
    {
        initial,
        running,
        cancelled,
    };

    // If thread_safe, a strand wrapping inner_pool_ex_, otherwise inner_pool_ex_
    asio::any_io_executor pool_ex_;

    // executor to be used by connections
    asio::any_io_executor conn_ex_;

    // Rest of the parameters
    internal_pool_params params_;

    // State
    state_t state_{state_t::initial};
    std::list<node_type> all_conns_;
    shared_state_type shared_st_;
    wait_group wait_gp_;
    timer_type cancel_timer_;
    const pipeline_request reset_pipeline_req_{make_reset_pipeline()};

    std::shared_ptr<this_type> shared_from_this_wrapper()
    {
        // Some compilers get confused without this explicit cast
        return static_cast<std::enable_shared_from_this<this_type>*>(this)->shared_from_this();
    }

    // Do we have room for a new connection?
    // Don't create new connections if we have other connections pending
    // (i.e. being connected, reset... ) - otherwise pool size increases
    // for no reason when there is no connectivity.
    bool can_create_connection() const
    {
        return all_conns_.size() < params_.max_size && shared_st_.num_pending_connections == 0u;
    }

    void create_connection()
    {
        all_conns_.emplace_back(params_, pool_ex_, conn_ex_, shared_st_, &reset_pipeline_req_);
        wait_gp_.run_task(all_conns_.back().async_run(asio::deferred));
    }

    template <class OpSelf>
    void enter_strand(OpSelf& self)
    {
        asio::dispatch(asio::bind_executor(pool_ex_, std::move(self)));
    }

    template <class OpSelf>
    void exit_strand(OpSelf& self)
    {
        asio::post(get_executor(), std::move(self));
    }

    struct run_op
    {
        int resume_point_{0};
        std::shared_ptr<this_type> obj_;
        asio::cancellation_slot cancel_slot_;

        run_op(std::shared_ptr<this_type> obj, asio::cancellation_slot slot) noexcept
            : obj_(std::move(obj)), cancel_slot_(slot)
        {
        }

        struct cancel_handler
        {
            this_type* self;

            void operator()(asio::cancellation_type_t type) const
            {
                if (run_supports_cancel_type(type))
                    self->cancel();
            }
        };

        template <class Self>
        void operator()(Self& self, error_code ec = {})
        {
            boost::ignore_unused(ec);

            switch (resume_point_)
            {
            case 0:
                // Emplace a cancellation handler, if required
                if (cancel_slot_.is_connected())
                {
                    cancel_slot_.template emplace<cancel_handler>(cancel_handler{obj_.get()});
                }

                // Ensure we run within the strand
                if (obj_->params_.thread_safe)
                {
                    BOOST_MYSQL_YIELD(resume_point_, 1, obj_->enter_strand(self))
                }

                // Check that we're not running and set the state adequately
                BOOST_ASSERT(obj_->state_ == state_t::initial);
                obj_->state_ = state_t::running;

                // Create the initial connections
                for (std::size_t i = 0; i < obj_->params_.initial_size; ++i)
                    obj_->create_connection();

                // Wait for the cancel notification to arrive.
                BOOST_MYSQL_YIELD(resume_point_, 2, obj_->cancel_timer_.async_wait(std::move(self)))

                // Ensure we run within the strand
                if (obj_->params_.thread_safe)
                {
                    BOOST_MYSQL_YIELD(resume_point_, 3, obj_->enter_strand(self))
                }

                // Deliver the cancel notification to all other tasks
                obj_->state_ = state_t::cancelled;
                for (auto& conn : obj_->all_conns_)
                    conn.cancel();
                obj_->shared_st_.pending_requests.notify_all();

                // Wait for all connection tasks to exit
                BOOST_MYSQL_YIELD(resume_point_, 4, obj_->wait_gp_.async_wait(std::move(self)))

                // Done
                cancel_slot_.clear();
                obj_.reset();
                self.complete(error_code());
            }
        }
    };

    struct get_connection_op
    {
        int resume_point_{0};
        std::shared_ptr<this_type> obj_;
        diagnostics* diag_;
        error_code result_ec_;
        node_type* result_conn_{};
        std::shared_ptr<asio::cancellation_signal> sig_;
        bool has_waited_{false};

        get_connection_op(
            std::shared_ptr<this_type> obj,
            diagnostics* diag,
            std::shared_ptr<asio::cancellation_signal> sig
        ) noexcept
            : obj_(std::move(obj)), diag_(diag), sig_(std::move(sig))
        {
        }

        bool thread_safe() const { return obj_->params_.thread_safe; }

        template <class Self>
        void do_complete(Self& self)
        {
            auto wr = result_ec_ ? ConnectionWrapper() : ConnectionWrapper(*result_conn_, std::move(obj_));
            obj_.reset();
            sig_.reset();
            self.complete(result_ec_, std::move(wr));
        }

        template <class Self>
        void operator()(Self& self, error_code = {})
        {
            switch (resume_point_)
            {
            case 0:
                // This op supports total cancellation. Must be explicitly enabled,
                // as composed ops only support terminal cancellation by default.
                self.reset_cancellation_state(asio::enable_total_cancellation());

                // Clear diagnostics
                if (diag_)
                    diag_->clear();

                // Enter the strand
                if (thread_safe())
                {
                    BOOST_MYSQL_YIELD(resume_point_, 1, obj_->enter_strand(self))
                }

                // This loop guards us against possible race conditions
                // between waiting on the pending request timer and getting the
                // connection
                while (true)
                {
                    // If we're not running yet, or were cancelled, just return
                    if (obj_->state_ == state_t::initial)
                    {
                        result_ec_ = client_errc::pool_not_running;
                        break;
                    }
                    else if (obj_->state_ == state_t::cancelled)
                    {
                        // The pool was cancelled
                        result_ec_ = asio::error::operation_aborted;
                        break;
                    }
                    else if (get_connection_supports_cancel_type(self.cancelled()))
                    {
                        // The operation was cancelled. Try to provide diagnostics
                        result_ec_ = asio::error::operation_aborted;
                        if (diag_)
                            *diag_ = obj_->shared_st_.last_connect_diag;
                        break;
                    }

                    // Try to get a connection
                    if (!obj_->shared_st_.idle_list.empty())
                    {
                        // There was a connection
                        result_conn_ = &obj_->shared_st_.idle_list.front();
                        result_conn_->mark_as_in_use();
                        break;
                    }

                    // No luck. If there is room for more connections, create one.
                    if (obj_->can_create_connection())
                    {
                        obj_->create_connection();
                    }

                    // Wait to be notified, or until a timeout happens
                    if (thread_safe())
                    {
                        BOOST_MYSQL_YIELD(
                            resume_point_,
                            2,
                            obj_->shared_st_.pending_requests.async_wait(
                                asio::bind_executor(obj_->pool_ex_, std::move(self))
                            )
                        )
                    }
                    else
                    {
                        BOOST_MYSQL_YIELD(
                            resume_point_,
                            3,
                            obj_->shared_st_.pending_requests.async_wait(std::move(self))
                        )
                    }

                    // Remember that we have waited, so completions are dispatched correctly
                    has_waited_ = true;
                }

                // Perform any required dispatching before completing
                if (thread_safe())
                {
                    // Exit the strand
                    BOOST_MYSQL_YIELD(resume_point_, 4, obj_->exit_strand(self))
                }
                else if (!has_waited_)
                {
                    // This is an immediate completion
                    BOOST_MYSQL_YIELD(
                        resume_point_,
                        5,
                        asio::async_immediate(self.get_io_executor(), std::move(self))
                    )
                }

                // Done
                do_complete(self);
            }
        }
    };

public:
    basic_pool_impl(pool_executor_params&& ex_params, pool_params&& params)
        : pool_ex_(
              params.thread_safe ? asio::make_strand(std::move(ex_params.pool_executor))
                                 : std::move(ex_params.pool_executor)
          ),
          conn_ex_(std::move(ex_params.connection_executor)),
          params_(make_internal_pool_params(std::move(params))),
          shared_st_(pool_ex_),
          wait_gp_(pool_ex_),
          cancel_timer_(pool_ex_, (std::chrono::steady_clock::time_point::max)())
    {
    }

    using executor_type = asio::any_io_executor;

    executor_type get_executor() { return params_.thread_safe ? strand().get_inner_executor() : pool_ex_; }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_run(CompletionToken&& token)
    {
        // Completely disable composed cancellation handling, as it's not what we want
        auto slot = asio::get_associated_cancellation_slot(token);
        auto token_without_slot = asio::bind_cancellation_slot(
            asio::cancellation_slot(),
            std::forward<CompletionToken>(token)
        );

        // Initiate passing the original token's slot manually
        return asio::async_compose<decltype(token_without_slot), void(error_code)>(
            run_op(shared_from_this_wrapper(), slot),
            token_without_slot,
            pool_ex_
        );
    }

    // Not thread-safe
    void cancel_unsafe() { cancel_timer_.expires_at((std::chrono::steady_clock::time_point::min)()); }

    void cancel()
    {
        if (params_.thread_safe)
        {
            // A handler to be passed to dispatch. Binds the executor
            // and keeps the pool alive
            struct dispatch_handler
            {
                std::shared_ptr<this_type> pool_ptr;

                using executor_type = asio::any_io_executor;
                executor_type get_executor() const noexcept { return pool_ptr->strand(); }

                void operator()() const { pool_ptr->cancel_unsafe(); }
            };

            asio::dispatch(dispatch_handler{shared_from_this_wrapper()});
        }
        else
        {
            cancel_unsafe();
        }
    }

    void return_connection(node_type& node, bool should_reset) noexcept
    {
        // This is safe to be called from any thread
        node.mark_as_collectable(should_reset);

        // The notification isn't thread-safe
        if (params_.thread_safe)
        {
            // A handler to be passed to dispatch. Binds the executor
            // and keeps the pool alive
            struct dispatch_handler
            {
                std::shared_ptr<this_type> pool_ptr;
                node_type* node_ptr;

                using executor_type = asio::any_io_executor;
                executor_type get_executor() const noexcept { return pool_ptr->strand(); }

                void operator()() const { node_ptr->notify_collectable(); }
            };

            // If, for any reason, this notification fails, the connection will
            // be collected when the next ping is due.
            try
            {
                asio::dispatch(dispatch_handler{shared_from_this_wrapper(), &node});
            }
            catch (...)
            {
            }
        }
        else
        {
            node.notify_collectable();
        }
    }

    // Cancel handler to use for get_connection in thread-safe mode.
    // This imitates what Asio does for composed ops
    struct get_connection_cancel_handler
    {
        // The child cancellation signal to be emitted.
        // Lifetime managed by the get_connection composed op
        std::weak_ptr<asio::cancellation_signal> sig_ptr;

        // The pool, required to get the strand
        std::shared_ptr<this_type> self;

        get_connection_cancel_handler(
            std::weak_ptr<asio::cancellation_signal> sig_ptr,
            std::shared_ptr<this_type> self
        ) noexcept
            : sig_ptr(std::move(sig_ptr)), self(std::move(self))
        {
        }

        void operator()(asio::cancellation_type_t type)
        {
            // The handler that should run through the strand
            struct dispatch_handler
            {
                std::weak_ptr<asio::cancellation_signal> sig_ptr;
                std::shared_ptr<this_type> self;
                asio::cancellation_type_t cancel_type;

                using executor_type = asio::any_io_executor;
                executor_type get_executor() const { return self->strand(); }

                void operator()()
                {
                    // If the operation hans't completed yet
                    if (auto signal = sig_ptr.lock())
                    {
                        // Emit the child signal, which effectively causes the op to be cancelled
                        signal->emit(cancel_type);
                    }
                }
            };

            if (get_connection_supports_cancel_type(type))
            {
                asio::dispatch(dispatch_handler{std::move(sig_ptr), std::move(self), type});
            }
        }
    };

    struct get_connection_initiation
    {
        template <class Handler>
        void operator()(Handler&& handler, diagnostics* diag, std::shared_ptr<this_type> self)
        {
            // In thread-safe mode, the cancel handler must be run through the strand
            auto slot = asio::get_associated_cancellation_slot(handler);
            if (self->params().thread_safe && slot.is_connected())
            {
                // Create the child signal
                auto sig = std::make_shared<asio::cancellation_signal>();

                // The original slot will call the handler, which dispatches to the strand
                slot.template emplace<get_connection_cancel_handler>(sig, self);

                // Bind the handler to the new slot
                auto bound_handler = asio::bind_cancellation_slot(
                    sig->slot(),
                    std::forward<Handler>(handler)
                );

                // Start
                asio::async_compose<decltype(bound_handler), void(error_code, ConnectionWrapper)>(
                    get_connection_op(self, diag, std::move(sig)),
                    bound_handler,
                    self->pool_ex_
                );
            }
            else
            {
                // Just start
                asio::async_compose<Handler, void(error_code, ConnectionWrapper)>(
                    get_connection_op(self, diag, nullptr),
                    handler,
                    self->pool_ex_
                );
            }
        }
    };

    template <class CompletionToken>
    auto async_get_connection(diagnostics* diag, CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(error_code, ConnectionWrapper)>(
            get_connection_initiation{},
            token,
            std::move(diag),
            shared_from_this_wrapper()
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code, ConnectionWrapper)>(
            get_connection_initiation{},
            token,
            std::move(diag),  // get diagnostics* instead of diagnostics*&. Helps test tooling
            shared_from_this_wrapper()
        );
    }

    asio::strand<asio::any_io_executor> strand()
    {
        BOOST_ASSERT(params_.thread_safe);
        return *pool_ex_.template target<asio::strand<asio::any_io_executor>>();
    }

    // Exposed for testing
    static bool run_supports_cancel_type(asio::cancellation_type_t v)
    {
        // run doesn't support total, as the pool state is always modified
        return !!(v & (asio::cancellation_type_t::partial | asio::cancellation_type_t::terminal));
    }

    static bool get_connection_supports_cancel_type(asio::cancellation_type_t v)
    {
        // get_connection supports all cancel types
        return !!(
            v & (asio::cancellation_type_t::partial | asio::cancellation_type_t::total |
                 asio::cancellation_type_t::terminal)
        );
    }

    std::list<node_type>& nodes() noexcept { return all_conns_; }
    shared_state_type& shared_state() noexcept { return shared_st_; }
    internal_pool_params& params() noexcept { return params_; }
    asio::any_io_executor connection_ex() noexcept { return conn_ex_; }
    const pipeline_request& reset_pipeline_request() const { return reset_pipeline_req_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
