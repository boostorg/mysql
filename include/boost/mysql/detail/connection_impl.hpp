//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_IMPL_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_IMPL_HPP

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
#include <boost/mysql/detail/any_address.hpp>
#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/any_stream.hpp>
#include <boost/mysql/detail/any_stream_impl.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
#include <boost/mysql/detail/owning_connect_params.hpp>
#include <boost/mysql/detail/run_algo.hpp>
#include <boost/mysql/detail/typing/get_type_index.hpp>
#include <boost/mysql/detail/variant_stream.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/mp11/integer_sequence.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <tuple>

namespace boost {
namespace mysql {

// Forward decl
template <class... StaticRow>
class static_execution_state;

namespace detail {

// Forward decl
class connection_state;

// Helper typedefs
template <class T>
using any_handler = asio::any_completion_handler<void(error_code, T)>;

using any_void_handler = asio::any_completion_handler<void(error_code)>;

//
// execution helpers
//
template <class... T, std::size_t... I>
std::array<field_view, sizeof...(T)> tuple_to_array_impl(const std::tuple<T...>& t, mp11::index_sequence<I...>) noexcept
{
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

class connection_impl
{
    std::unique_ptr<any_stream> stream_;
    std::unique_ptr<connection_state> st_;

    // Misc
    BOOST_MYSQL_DECL
    static std::vector<field_view>& get_shared_fields(connection_state& st) noexcept;

    // Generic algorithm
    struct run_algo_initiation
    {
        template <class Handler, class AlgoParams>
        void operator()(Handler&& handler, any_stream* stream, connection_state* st, AlgoParams params)
        {
            async_run_algo(*stream, *st, params, std::forward<Handler>(handler));
        }
    };

    // Connect
    template <class Stream>
    static void set_endpoint(
        any_stream& stream,
        const typename Stream::lowest_layer_type::endpoint_type& endpoint
    )
    {
        static_cast<any_stream_impl<Stream>&>(stream).set_endpoint(endpoint);
    }

    inline static void set_address(any_stream& stream, any_address address)
    {
        static_cast<variant_stream&>(stream).set_address(address);
    }

    template <class Stream>
    struct connect_initiation
    {
        template <class Handler>
        void operator()(
            Handler&& handler,
            any_stream* stream,
            connection_state* st,
            const typename Stream::lowest_layer_type::endpoint_type& endpoint,
            handshake_params params,
            diagnostics* diag
        )
        {
            set_endpoint<Stream>(*stream, endpoint);
            async_run_algo(*stream, *st, connect_algo_params{diag, params}, std::forward<Handler>(handler));
        }
    };

    struct connect_v2_initiation
    {
        template <class Handler>
        void operator()(
            Handler&& handler,
            any_stream* stream,
            connection_state* st,
            any_address address,
            handshake_params hparams,
            diagnostics* diag
        )
        {
            static_cast<detail::variant_stream*>(stream)->set_address(address);
            async_run_algo(*stream, *st, connect_algo_params{diag, hparams}, std::forward<Handler>(handler));
        }
    };

    // execute
    struct initiate_execute
    {
        template <class Handler, class ExecutionRequest>
        void operator()(
            Handler&& handler,
            any_stream* stream,
            connection_state* st,
            const ExecutionRequest& req,
            execution_processor* proc,
            diagnostics* diag
        )
        {
            auto getter = make_request_getter(req, get_shared_fields(*st));
            async_run_algo(
                *stream,
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
            any_stream* stream,
            connection_state* st,
            const ExecutionRequest& req,
            execution_processor* proc,
            diagnostics* diag
        )
        {
            auto getter = make_request_getter(req, get_shared_fields(*st));
            async_run_algo(
                *stream,
                *st,
                start_execution_algo_params{diag, getter.get(), proc},
                std::forward<Handler>(handler)
            );
        }
    };

public:
    BOOST_MYSQL_DECL connection_impl(std::size_t read_buff_size, std::unique_ptr<any_stream> stream);
    connection_impl(const connection_impl&) = delete;
    BOOST_MYSQL_DECL connection_impl(connection_impl&&) noexcept;
    connection_impl& operator=(const connection_impl&) = delete;
    BOOST_MYSQL_DECL connection_impl& operator=(connection_impl&&) noexcept;
    BOOST_MYSQL_DECL ~connection_impl();

    any_stream& stream() noexcept
    {
        BOOST_ASSERT(stream_);
        return *stream_;
    }

    const any_stream& stream() const noexcept
    {
        BOOST_ASSERT(stream_);
        return *stream_;
    }

    BOOST_MYSQL_DECL metadata_mode meta_mode() const noexcept;
    BOOST_MYSQL_DECL void set_meta_mode(metadata_mode v) noexcept;
    BOOST_MYSQL_DECL std::vector<field_view>& get_shared_fields() noexcept;
    BOOST_MYSQL_DECL bool ssl_active() const noexcept;

    // Generic algorithm
    template <class AlgoParams, class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, completion_signature_t<AlgoParams>)
    async_run(AlgoParams params, CompletionToken&& token)
    {
        return asio::async_initiate<CompletionToken, completion_signature_t<AlgoParams>>(
            run_algo_initiation(),
            token,
            stream_.get(),
            st_.get(),
            params
        );
    }

    template <class AlgoParams>
    typename AlgoParams::result_type run(AlgoParams params, error_code& ec)
    {
        return run_algo(*stream_, *st_, params, ec);
    }

    // Connect. This handles casting to the corresponding endpoint_type, if required
    template <class Stream>
    void connect(
        const typename Stream::lowest_layer_type::endpoint_type& endpoint,
        const handshake_params& params,
        error_code& err,
        diagnostics& diag
    )
    {
        set_endpoint<Stream>(*stream_, endpoint);
        run_algo(*stream_, *st_, connect_algo_params{&diag, params}, err);
    }

    template <class Stream, class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const typename Stream::lowest_layer_type::endpoint_type& endpoint,
        const handshake_params& params,
        diagnostics& diag,
        CompletionToken&& token
    )
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            connect_initiation<Stream>(),
            token,
            stream_.get(),
            st_.get(),
            endpoint,
            params,
            &diag
        );
    }

    // Connect v2
    void connect_v2(const connect_params& params, error_code& err, diagnostics& diag)
    {
        const auto& impl = access::get_impl(params);
        set_address(*stream_, impl.to_address());
        run_algo(*stream_, *st_, connect_algo_params{&diag, impl.to_handshake_params()}, err);
    }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect_v2(const connect_params& params, diagnostics& diag, CompletionToken&& token)
    {
        auto params_and_strings = owning_connect_params::create(params);
        return async_connect_v2(
            params_and_strings.address,
            params_and_strings.hparams,
            diag,
            asio::consign(std::forward<CompletionToken>(token), std::move(params_and_strings.string_buffer))
        );
    }

    template <class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect_v2(
        any_address addr,
        const handshake_params& params,
        diagnostics& diag,
        CompletionToken&& token
    )
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            connect_v2_initiation(),
            token,
            stream_.get(),
            st_.get(),
            addr,
            params,
            &diag
        );
    }

    // Handshake
    handshake_algo_params make_params_handshake(const handshake_params& params, diagnostics& diag)
        const noexcept
    {
        return {&diag, params};
    }

    // Execute
    template <class ExecutionRequest, class ResultsType>
    void execute(const ExecutionRequest& req, ResultsType& result, error_code& err, diagnostics& diag)
    {
        auto getter = make_request_getter(req, get_shared_fields(*st_));
        run_algo(
            *stream_,
            *st_,
            execute_algo_params{&diag, getter.get(), &access::get_impl(result).get_interface()},
            err
        );
    }

    template <class ExecutionRequest, class ResultsType, class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_execute(ExecutionRequest&& req, ResultsType& result, diagnostics& diag, CompletionToken&& token)
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_execute(),
            token,
            stream_.get(),
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
        run_algo(
            *stream_,
            *st_,
            start_execution_algo_params{&diag, getter.get(), &access::get_impl(exec_st).get_interface()},
            err
        );
    }

    template <class ExecutionRequest, class ExecutionStateType, class CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_start_execution(
        ExecutionRequest&& req,
        ExecutionStateType& exec_st,
        diagnostics& diag,
        CompletionToken&& token
    )
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_start_execution(),
            token,
            stream_.get(),
            st_.get(),
            std::forward<ExecutionRequest>(req),
            &access::get_impl(exec_st).get_interface(),
            &diag
        );
    }

    // Read some rows (dynamic)
    read_some_rows_dynamic_algo_params make_params_read_some_rows(execution_state& st, diagnostics& diag)
        const noexcept
    {
        return {&diag, &access::get_impl(st).get_interface()};
    }

    // Read some rows (static)
    template <class SpanRowType, class... RowType>
    read_some_rows_algo_params make_params_read_some_rows(
        static_execution_state<RowType...>& exec_st,
        span<SpanRowType> output,
        diagnostics& diag
    ) const noexcept
    {
        constexpr std::size_t index = get_type_index<SpanRowType, RowType...>();
        static_assert(index != index_not_found, "SpanRowType must be one of the types returned by the query");
        return {&diag, &access::get_impl(exec_st).get_interface(), output_ref(output, index)};
    }

    // Read resultset head
    template <class ExecutionStateType>
    read_resultset_head_algo_params make_params_read_resultset_head(ExecutionStateType& st, diagnostics& diag)
        const noexcept
    {
        return {&diag, &detail::access::get_impl(st).get_interface()};
    }

    // Close statement
    close_statement_algo_params make_params_close_statement(statement stmt, diagnostics& diag) const noexcept
    {
        return {&diag, stmt.id()};
    }

    // Ping
    ping_algo_params make_params_ping(diagnostics& diag) const noexcept { return {&diag}; }

    // Reset connection
    reset_connection_algo_params make_params_reset_connection(diagnostics& diag) const noexcept
    {
        return {&diag};
    }

    // Quit connection
    quit_connection_algo_params make_params_quit(diagnostics& diag) const noexcept { return {&diag}; }

    // Close connection
    close_connection_algo_params make_params_close(diagnostics& diag) const noexcept { return {&diag}; }

    // TODO: get rid of this
    BOOST_MYSQL_DECL
    diagnostics& shared_diag() noexcept;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/connection_impl.ipp>
#endif

#endif
