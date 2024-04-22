//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_STATE_API_IPP
#define BOOST_MYSQL_IMPL_CONNECTION_STATE_API_IPP

#pragma once

#include <boost/mysql/detail/connection_state_api.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state.hpp>

std::vector<boost::mysql::field_view>& boost::mysql::detail::connection_state_api::get_shared_fields(
) noexcept
{
    return st_->data().shared_fields;
}

boost::mysql::detail::connection_state_api::connection_state_api(
    std::size_t read_buff_size,
    bool stream_supports_ssl
)
    : st_(new connection_state(read_buff_size, stream_supports_ssl))
{
}

void boost::mysql::detail::connection_state_api::pimpl_deleter::operator()(connection_state* st
) const noexcept
{
    delete st;
}

boost::mysql::metadata_mode boost::mysql::detail::connection_state_api::meta_mode() const noexcept
{
    return st_->data().meta_mode;
}

void boost::mysql::detail::connection_state_api::set_meta_mode(metadata_mode v) noexcept
{
    st_->data().meta_mode = v;
}

bool boost::mysql::detail::connection_state_api::ssl_active() const noexcept
{
    return st_->data().ssl_active();
}

bool boost::mysql::detail::connection_state_api::backslash_escapes() const noexcept
{
    return st_->data().backslash_escapes;
}

boost::mysql::diagnostics& boost::mysql::detail::connection_state_api::shared_diag() noexcept
{
    return st_->data().shared_diag;
}

boost::system::result<boost::mysql::character_set> boost::mysql::detail::connection_state_api::
    current_character_set() const noexcept
{
    const auto* res = st_->data().charset_ptr();
    if (res == nullptr)
        return client_errc::unknown_character_set;
    return *res;
}

template <class AlgoParams>
boost::mysql::detail::any_resumable_ref boost::mysql::detail::connection_state_api::setup(
    const AlgoParams& params
)
{
    st_->setup(params);
}

template <class AlgoParams>
typename AlgoParams::result_type boost::mysql::detail::connection_state_api::get_result() const noexcept
{
    return st_->result<AlgoParams>();
}

#ifdef BOOST_MYSQL_SEPARATE_COMPILATION

#define BOOST_MYSQL_INSTANTIATE_SETUP(op_params_type) \
    template any_resumable_ref connection_state_api::setup<op_params_type>(const op_params_type&);

#define BOOST_MYSQL_INSTANTIATE_GET_RESULT(op_params_type) \
    template op_params_type::result_type connection_state_api::get_result<op_params_type>() const noexcept;

namespace boost {
namespace mysql {
namespace detail {

BOOST_MYSQL_INSTANTIATE_SETUP(connect_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(handshake_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(execute_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(start_execution_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(read_resultset_head_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(read_some_rows_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(read_some_rows_dynamic_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(prepare_statement_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(close_statement_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(set_character_set_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(ping_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(reset_connection_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(quit_connection_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(close_connection_algo_params)

BOOST_MYSQL_INSTANTIATE_GET_RESULT(read_some_rows_algo_params)
BOOST_MYSQL_INSTANTIATE_GET_RESULT(read_some_rows_dynamic_algo_params)
BOOST_MYSQL_INSTANTIATE_GET_RESULT(prepare_statement_algo_params)

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

#endif
