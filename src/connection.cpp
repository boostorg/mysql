//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/connect_params_helpers.hpp>
#include <boost/mysql/detail/engine.hpp>

#include "mycosql_internal/sansio/connection_state.hpp"
#include "mycosql_internal/variant_stream.hpp"

using namespace boost::mysql;
namespace capy = boost::capy;

// impl_t definition
struct any_connection::impl_t
{
    detail::connection_state st;
    detail::engine eng;
};

// Construction and destruction
static std::unique_ptr<detail::mysql_stream> create_variant_stream(capy::execution_context& ctx)
{
    return std::unique_ptr<detail::mysql_stream>{new detail::variant_stream(ctx)};
}

any_connection::any_connection(capy::execution_context& ctx, any_connection_params params)
{
    auto stream = create_variant_stream(ctx);
    impl_.reset(new impl_t{
        {params.initial_buffer_size, params.max_buffer_size, stream->supports_tls()},
        std::move(stream),
    });
}

any_connection& any_connection::operator=(any_connection&& rhs) noexcept = default;
any_connection::~any_connection() = default;

// Getters
bool any_connection::uses_ssl() const noexcept { return impl_->st.data().tls_active; }

bool any_connection::backslash_escapes() const noexcept { return impl_->st.data().backslash_escapes; }

boost::system::result<character_set> any_connection::current_character_set() const noexcept
{
    auto& data = impl_->st.data();
    if (data.current_charset.name == nullptr)
        return error_code(client_errc::unknown_character_set);
    return data.current_charset;
}

boost::system::result<format_options> any_connection::format_opts() const noexcept
{
    auto res = current_character_set();
    if (res.has_error())
        return res.error();
    return format_options{res.value(), backslash_escapes()};
}

metadata_mode any_connection::meta_mode() const noexcept { return impl_->st.data().meta_mode; }

void any_connection::set_meta_mode(metadata_mode v) noexcept { impl_->st.data().meta_mode = v; }

boost::optional<std::uint32_t> any_connection::connection_id() const noexcept
{
    auto id = impl_->st.data().connection_id;
    if (id == 0)
        return {};
    return id;
}

// Private helpers
std::vector<field_view>& any_connection::shared_fields() { return impl_->st.data().shared_fields; }

// I/O operations
capy::io_task<diagnostics> any_connection::connect(const connect_params& params)
{
    detail::connect_algo_params p{
        &params.server_address,
        detail::make_hparams(params),
        params.server_address.type() == address_type::unix_path
    };
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}

capy::io_task<diagnostics> any_connection::execute_impl(
    detail::any_execution_request req,
    detail::execution_processor* proc
)
{
    detail::execute_algo_params p{req, proc};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}

capy::io_task<diagnostics, statement> any_connection::prepare_statement(std::string_view stmt)
{
    detail::prepare_statement_algo_params p{stmt};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag), impl_->st.result<detail::prepare_statement_algo_params>()};
}

capy::io_task<diagnostics> any_connection::close_statement(const statement& stmt)
{
    detail::close_statement_algo_params p{stmt.id()};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}

capy::io_task<diagnostics> any_connection::start_execution_impl(
    detail::any_execution_request req,
    detail::execution_processor* proc
)
{
    detail::start_execution_algo_params p{req, proc};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}

capy::io_task<diagnostics, rows_view> any_connection::read_some_rows(execution_state& st)
{
    detail::read_some_rows_dynamic_algo_params p{&detail::access::get_impl(st)};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag), impl_->st.result<detail::read_some_rows_dynamic_algo_params>()};
}

capy::io_task<diagnostics, std::size_t> any_connection::read_some_rows_impl(
    detail::execution_processor* proc,
    detail::output_ref output
)
{
    detail::read_some_rows_algo_params p{proc, output};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag), impl_->st.result<detail::read_some_rows_algo_params>()};
}

capy::io_task<diagnostics> any_connection::read_resultset_head_impl(detail::execution_processor* proc)
{
    detail::read_resultset_head_algo_params p{proc};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}

capy::io_task<diagnostics> any_connection::set_character_set(const character_set& charset)
{
    detail::set_character_set_algo_params p{charset};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}

capy::io_task<diagnostics> any_connection::ping()
{
    detail::ping_algo_params p{};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}

capy::io_task<diagnostics> any_connection::reset_connection()
{
    detail::reset_connection_algo_params p{};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}

capy::io_task<diagnostics> any_connection::close()
{
    detail::close_connection_algo_params p{};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}

capy::io_task<diagnostics> any_connection::run_pipeline(
    const pipeline_request& req,
    std::vector<stage_response>& res
)
{
    auto& req_impl = detail::access::get_impl(req);
    detail::run_pipeline_algo_params p{req_impl.buffer_, req_impl.stages_, &res};
    diagnostics diag;
    auto ref = impl_->st.setup(diag, p);
    auto [ec] = co_await impl_->eng.run(ref);
    co_return {ec, std::move(diag)};
}
