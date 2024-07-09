//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_IMPL_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_IMPL_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connect_params_helpers.hpp>
#include <boost/mysql/detail/engine.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/system/result.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <tuple>
#include <utility>

namespace boost {
namespace mysql {

// Forward decl
template <class... StaticRow>
class static_execution_state;

struct character_set;
class pipeline_request;

namespace detail {

// Forward decl
class connection_state;

//
// Helpers to interact with connection_state, without including its definition
//
struct connection_state_deleter
{
    BOOST_MYSQL_DECL void operator()(connection_state*) const;
};

BOOST_MYSQL_DECL std::vector<field_view>& get_shared_fields(connection_state&);

template <class AlgoParams>
any_resumable_ref setup(connection_state&, const AlgoParams&);

// Note: AlgoParams should have !is_void_result
template <class AlgoParams>
typename AlgoParams::result_type get_result(const connection_state&);

//
// execution helpers
//
template <class... T, std::size_t... I>
std::array<field_view, sizeof...(T)> tuple_to_array_impl(const std::tuple<T...>& t, mp11::index_sequence<I...>) noexcept
{
    boost::ignore_unused(t);  // MSVC gets confused if sizeof...(T) == 0
    return std::array<field_view, sizeof...(T)>{{to_field(std::get<I>(t))...}};
}

template <class... T>
std::array<field_view, sizeof...(T)> tuple_to_array(const std::tuple<T...>& t) noexcept
{
    return tuple_to_array_impl(t, mp11::make_index_sequence<sizeof...(T)>());
}

struct query_request_getter
{
    any_execution_request value;
    any_execution_request get() const noexcept { return value; }
};
inline query_request_getter make_request_getter(string_view q, std::vector<field_view>&) noexcept
{
    return query_request_getter{q};
}

struct stmt_it_request_getter
{
    statement stmt;
    span<const field_view> params;  // Points into the connection state's shared fields

    any_execution_request get() const noexcept { return any_execution_request(stmt, params); }
};

template <class FieldViewFwdIterator>
inline stmt_it_request_getter make_request_getter(
    const bound_statement_iterator_range<FieldViewFwdIterator>& req,
    std::vector<field_view>& shared_fields
)
{
    auto& impl = access::get_impl(req);
    shared_fields.assign(impl.first, impl.last);
    return {impl.stmt, shared_fields};
}

template <std::size_t N>
struct stmt_tuple_request_getter
{
    statement stmt;
    std::array<field_view, N> params;

    any_execution_request get() const noexcept { return any_execution_request(stmt, params); }
};
template <class WritableFieldTuple>
stmt_tuple_request_getter<std::tuple_size<WritableFieldTuple>::value>
make_request_getter(const bound_statement_tuple<WritableFieldTuple>& req, std::vector<field_view>&)
{
    auto& impl = access::get_impl(req);
    return {impl.stmt, tuple_to_array(impl.params)};
}

//
// helpers to run algos
//

template <class AlgoParams>
using has_void_result = std::is_same<typename AlgoParams::result_type, void>;

template <class AlgoParams, bool is_void>
struct completion_signature_impl;

template <class AlgoParams>
struct completion_signature_impl<AlgoParams, true>
{
    // Using typedef to workaround a msvc 14.1 bug
    typedef void(type)(error_code);
};

template <class AlgoParams>
struct completion_signature_impl<AlgoParams, false>
{
    // Using typedef to workaround a msvc 14.1 bug
    typedef void(type)(error_code, typename AlgoParams::result_type);
};

template <class AlgoParams>
using completion_signature_t = typename completion_signature_impl<
    AlgoParams,
    has_void_result<AlgoParams>::value>::type;

// Intermediate handler
template <class AlgoParams, class Handler>
struct generic_algo_handler
{
    static_assert(!has_void_result<AlgoParams>::value, "Internal error: result_type should be non-void");

    using result_t = typename AlgoParams::result_type;

    template <class DeducedHandler>
    generic_algo_handler(DeducedHandler&& h, connection_state& st)
        : final_handler(std::forward<DeducedHandler>(h)), st(&st)
    {
    }

    void operator()(error_code ec)
    {
        std::move(final_handler)(ec, ec ? result_t{} : get_result<AlgoParams>(*st));
    }

    Handler final_handler;  // needs to be accessed by associator
    connection_state* st;
};

class connection_impl
{
    std::unique_ptr<engine> engine_;
    std::unique_ptr<connection_state, connection_state_deleter> st_;

    // Generic algorithm
    template <class AlgoParams>
    typename AlgoParams::result_type run_impl(
        AlgoParams params,
        error_code& ec,
        std::true_type /* has_void_result */
    )
    {
        engine_->run(setup(*st_, params), ec);
    }

    template <class AlgoParams>
    typename AlgoParams::result_type run_impl(
        AlgoParams params,
        error_code& ec,
        std::false_type /* has_void_result */
    )
    {
        engine_->run(setup(*st_, params), ec);
        return get_result<AlgoParams>(*st_);
    }

    template <class AlgoParams, class Handler>
    static void async_run_impl(
        engine& eng,
        connection_state& st,
        AlgoParams params,
        Handler&& handler,
        std::true_type /* has_void_result */
    )
    {
        eng.async_run(setup(st, params), std::forward<Handler>(handler));
    }

    template <class AlgoParams, class Handler>
    static void async_run_impl(
        engine& eng,
        connection_state& st,
        AlgoParams params,
        Handler&& handler,
        std::false_type /* has_void_result */
    )
    {
        using intermediate_handler_t = generic_algo_handler<AlgoParams, typename std::decay<Handler>::type>;
        eng.async_run(setup(st, params), intermediate_handler_t(std::forward<Handler>(handler), st));
    }

    template <class AlgoParams, class Handler>
    static void async_run_impl(engine& eng, connection_state& st, AlgoParams params, Handler&& handler)
    {
        async_run_impl(eng, st, params, std::forward<Handler>(handler), has_void_result<AlgoParams>{});
    }

    struct run_algo_initiation
    {
        template <class Handler, class AlgoParams>
        void operator()(Handler&& handler, engine* eng, connection_state* st, AlgoParams params)
        {
            async_run_impl(*eng, *st, params, std::forward<Handler>(handler));
        }
    };

    // Connect
    static connect_algo_params make_params_connect(diagnostics& diag, const handshake_params& params)
    {
        return connect_algo_params{&diag, params, false};
    }

    static connect_algo_params make_params_connect_v2(diagnostics& diag, const connect_params& params)
    {
        return connect_algo_params{
            &diag,
            make_hparams(params),
            params.server_address.type() == address_type::unix_path
        };
    }

    template <class EndpointType>
    struct initiate_connect
    {
        template <class Handler>
        void operator()(
            Handler&& handler,
            engine* eng,
            connection_state* st,
            const EndpointType& endpoint,
            handshake_params params,
            diagnostics* diag
        )
        {
            eng->set_endpoint(&endpoint);
            async_run_impl(*eng, *st, make_params_connect(*diag, params), std::forward<Handler>(handler));
        }
    };

    struct initiate_connect_v2
    {
        template <class Handler>
        void operator()(
            Handler&& handler,
            engine* eng,
            connection_state* st,
            const connect_params* params,
            diagnostics* diag
        )
        {
            eng->set_endpoint(&params->server_address);
            async_run_impl(*eng, *st, make_params_connect_v2(*diag, *params), std::forward<Handler>(handler));
        }
    };

    // execute
    struct initiate_execute
    {
        template <class Handler, class ExecutionRequest>
        void operator()(
            Handler&& handler,
            engine* eng,
            connection_state* st,
            const ExecutionRequest& req,
            execution_processor* proc,
            diagnostics* diag
        )
        {
            auto getter = make_request_getter(req, get_shared_fields(*st));
            async_run_impl(
                *eng,
                *st,
                execute_algo_params{diag, getter.get(), proc},
                std::forward<Handler>(handler)
            );
        }
    };

    // start execution
    struct initiate_start_execution
    {
        template <class Handler, class ExecutionRequest>
        void operator()(
            Handler&& handler,
            engine* eng,
            connection_state* st,
            const ExecutionRequest& req,
            execution_processor* proc,
            diagnostics* diag
        )
        {
            auto getter = make_request_getter(req, get_shared_fields(*st));
            async_run_impl(
                *eng,
                *st,
                start_execution_algo_params{diag, getter.get(), proc},
                std::forward<Handler>(handler)
            );
        }
    };

public:
    BOOST_MYSQL_DECL connection_impl(
        std::size_t read_buff_size,
        std::size_t max_buffer_size,
        std::unique_ptr<engine> eng
    );

    BOOST_MYSQL_DECL metadata_mode meta_mode() const;
    BOOST_MYSQL_DECL void set_meta_mode(metadata_mode m);
    BOOST_MYSQL_DECL bool ssl_active() const;
    BOOST_MYSQL_DECL bool backslash_escapes() const;
    BOOST_MYSQL_DECL system::result<character_set> current_character_set() const;
    BOOST_MYSQL_DECL diagnostics& shared_diag();  // TODO: get rid of this

    engine& get_engine()
    {
        BOOST_ASSERT(engine_);
        return *engine_;
    }

    const engine& get_engine() const
    {
        BOOST_ASSERT(engine_);
        return *engine_;
    }

    // Generic algorithm
    template <class AlgoParams>
    typename AlgoParams::result_type run(AlgoParams params, error_code& ec)
    {
        return run_impl(params, ec, has_void_result<AlgoParams>{});
    }

    template <class AlgoParams, class CompletionToken>
    auto async_run(AlgoParams params, CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, completion_signature_t<AlgoParams>>(
            run_algo_initiation(),
            token,
            engine_.get(),
            st_.get(),
            params
        ))
    {
        return asio::async_initiate<CompletionToken, completion_signature_t<AlgoParams>>(
            run_algo_initiation(),
            token,
            engine_.get(),
            st_.get(),
            params
        );
    }

    // Connect
    template <class EndpointType>
    void connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        error_code& err,
        diagnostics& diag
    )
    {
        engine_->set_endpoint(&endpoint);
        run(make_params_connect(diag, params), err);
    }

    void connect_v2(const connect_params& params, error_code& err, diagnostics& diag)
    {
        engine_->set_endpoint(&params.server_address);
        run(make_params_connect_v2(diag, params), err);
    }

    template <class EndpointType, class CompletionToken>
    auto async_connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        diagnostics& diag,
        CompletionToken&& token
    )
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_connect<EndpointType>(),
            token,
            engine_.get(),
            st_.get(),
            endpoint,
            params,
            &diag
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_connect<EndpointType>(),
            token,
            engine_.get(),
            st_.get(),
            endpoint,
            params,
            &diag
        );
    }

    template <class CompletionToken>
    auto async_connect_v2(const connect_params& params, diagnostics& diag, CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_connect_v2(),
            token,
            engine_.get(),
            st_.get(),
            &params,
            &diag
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_connect_v2(),
            token,
            engine_.get(),
            st_.get(),
            &params,
            &diag
        );
    }

    // Handshake
    handshake_algo_params make_params_handshake(const handshake_params& params, diagnostics& diag) const
    {
        return {&diag, params, false};
    }

    // Execute
    template <class ExecutionRequest, class ResultsType>
    void execute(const ExecutionRequest& req, ResultsType& result, error_code& err, diagnostics& diag)
    {
        auto getter = make_request_getter(req, get_shared_fields(*st_));
        run(execute_algo_params{&diag, getter.get(), &access::get_impl(result).get_interface()}, err);
    }

    template <class ExecutionRequest, class ResultsType, class CompletionToken>
    auto async_execute(
        ExecutionRequest&& req,
        ResultsType& result,
        diagnostics& diag,
        CompletionToken&& token
    )
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_execute(),
            token,
            engine_.get(),
            st_.get(),
            std::forward<ExecutionRequest>(req),
            &access::get_impl(result).get_interface(),
            &diag
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_execute(),
            token,
            engine_.get(),
            st_.get(),
            std::forward<ExecutionRequest>(req),
            &access::get_impl(result).get_interface(),
            &diag
        );
    }

    // Start execution
    template <class ExecutionRequest, class ExecutionStateType>
    void start_execution(
        const ExecutionRequest& req,
        ExecutionStateType& exec_st,
        error_code& err,
        diagnostics& diag
    )
    {
        auto getter = make_request_getter(req, get_shared_fields(*st_));
        run(start_execution_algo_params{&diag, getter.get(), &access::get_impl(exec_st).get_interface()},
            err);
    }

    template <class ExecutionRequest, class ExecutionStateType, class CompletionToken>
    auto async_start_execution(
        ExecutionRequest&& req,
        ExecutionStateType& exec_st,
        diagnostics& diag,
        CompletionToken&& token
    )
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_start_execution(),
            token,
            engine_.get(),
            st_.get(),
            std::forward<ExecutionRequest>(req),
            &access::get_impl(exec_st).get_interface(),
            &diag
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_start_execution(),
            token,
            engine_.get(),
            st_.get(),
            std::forward<ExecutionRequest>(req),
            &access::get_impl(exec_st).get_interface(),
            &diag
        );
    }

    // Read some rows (dynamic)
    read_some_rows_dynamic_algo_params make_params_read_some_rows(execution_state& st, diagnostics& diag)
        const
    {
        return {&diag, &access::get_impl(st).get_interface()};
    }

    // Read some rows (static)
    template <class SpanElementType, class ExecutionState>
    read_some_rows_algo_params make_params_read_some_rows_static(
        ExecutionState& exec_st,
        span<SpanElementType> output,
        diagnostics& diag
    ) const
    {
        return {
            &diag,
            &access::get_impl(exec_st).get_interface(),
            access::get_impl(exec_st).make_output_ref(output)
        };
    }

    // Read resultset head
    template <class ExecutionStateType>
    read_resultset_head_algo_params make_params_read_resultset_head(ExecutionStateType& st, diagnostics& diag)
        const
    {
        return {&diag, &detail::access::get_impl(st).get_interface()};
    }

    // Close statement
    close_statement_algo_params make_params_close_statement(statement stmt, diagnostics& diag) const
    {
        return {&diag, stmt.id()};
    }

    // Set character set
    set_character_set_algo_params make_params_set_character_set(
        const character_set& charset,
        diagnostics& diag
    ) const
    {
        return {&diag, charset};
    }

    // Ping
    ping_algo_params make_params_ping(diagnostics& diag) const { return {&diag}; }

    // Reset connection
    reset_connection_algo_params make_params_reset_connection(diagnostics& diag) const { return {&diag}; }

    // Quit connection
    quit_connection_algo_params make_params_quit(diagnostics& diag) const { return {&diag}; }

    // Close connection
    close_connection_algo_params make_params_close(diagnostics& diag) const { return {&diag}; }

    // Run pipeline. Separately compiled to avoid including the pipeline header here
    BOOST_MYSQL_DECL
    static run_pipeline_algo_params make_params_pipeline(
        const pipeline_request& req,
        std::vector<stage_response>& response,
        diagnostics& diag
    );
};

// To use some completion tokens, like deferred, in C++11, the old macros
// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE are no longer enough.
// Helper typedefs to reduce duplication
template <class AlgoParams, class CompletionToken>
using async_run_t = decltype(std::declval<connection_impl&>()
                                 .async_run(std::declval<AlgoParams>(), std::declval<CompletionToken>()));

template <class EndpointType, class CompletionToken>
using async_connect_t = decltype(std::declval<connection_impl&>().async_connect(
    std::declval<const EndpointType&>(),
    std::declval<const handshake_params&>(),
    std::declval<diagnostics&>(),
    std::declval<CompletionToken>()
));

template <class CompletionToken>
using async_connect_v2_t = decltype(std::declval<connection_impl&>().async_connect_v2(
    std::declval<const connect_params&>(),
    std::declval<diagnostics&>(),
    std::declval<CompletionToken>()
));

template <class ExecutionRequest, class ResultsType, class CompletionToken>
using async_execute_t = decltype(std::declval<connection_impl&>().async_execute(
    std::declval<ExecutionRequest>(),
    std::declval<ResultsType&>(),
    std::declval<diagnostics&>(),
    std::declval<CompletionToken>()
));

template <class ExecutionRequest, class ExecutionStateType, class CompletionToken>
using async_start_execution_t = decltype(std::declval<connection_impl&>().async_start_execution(
    std::declval<ExecutionRequest>(),
    std::declval<ExecutionStateType&>(),
    std::declval<diagnostics&>(),
    std::declval<CompletionToken>()
));

template <class CompletionToken>
using async_handshake_t = async_run_t<handshake_algo_params, CompletionToken>;

template <class CompletionToken>
using async_read_resultset_head_t = async_run_t<read_resultset_head_algo_params, CompletionToken>;

template <class CompletionToken>
using async_read_some_rows_dynamic_t = async_run_t<read_some_rows_dynamic_algo_params, CompletionToken>;

template <class CompletionToken>
using async_prepare_statement_t = async_run_t<prepare_statement_algo_params, CompletionToken>;

template <class CompletionToken>
using async_close_statement_t = async_run_t<close_statement_algo_params, CompletionToken>;

template <class CompletionToken>
using async_set_character_set_t = async_run_t<set_character_set_algo_params, CompletionToken>;

template <class CompletionToken>
using async_ping_t = async_run_t<ping_algo_params, CompletionToken>;

template <class CompletionToken>
using async_reset_connection_t = async_run_t<reset_connection_algo_params, CompletionToken>;

template <class CompletionToken>
using async_quit_connection_t = async_run_t<quit_connection_algo_params, CompletionToken>;

template <class CompletionToken>
using async_close_connection_t = async_run_t<close_connection_algo_params, CompletionToken>;

template <class CompletionToken>
using async_run_pipeline_t = async_run_t<run_pipeline_algo_params, CompletionToken>;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

// Propagate associated properties
namespace boost {
namespace asio {

template <template <class, class> class Associator, class AlgoParams, class Handler, class DefaultCandidate>
struct associator<Associator, mysql::detail::generic_algo_handler<AlgoParams, Handler>, DefaultCandidate>
    : Associator<Handler, DefaultCandidate>
{
    using mysql_handler = mysql::detail::generic_algo_handler<AlgoParams, Handler>;

    static typename Associator<Handler, DefaultCandidate>::type get(const mysql_handler& h)
    {
        return Associator<Handler, DefaultCandidate>::get(h.final_handler);
    }

    static auto get(const mysql_handler& h, const DefaultCandidate& c)
        -> decltype(Associator<Handler, DefaultCandidate>::get(h.final_handler, c))
    {
        return Associator<Handler, DefaultCandidate>::get(h.final_handler, c);
    }
};

}  // namespace asio
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/connection_impl.ipp>
#endif

#endif
