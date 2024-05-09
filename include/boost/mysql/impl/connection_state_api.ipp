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

std::vector<boost::mysql::field_view>& boost::mysql::detail::get_shared_fields(connection_state& st)
{
    return st.data().shared_fields;
}

boost::mysql::detail::connection_state_ptr boost::mysql::detail::create_connection_state(
    std::size_t read_buff_size,
    bool stream_supports_ssl
)
{
    return connection_state_ptr{new connection_state(read_buff_size, stream_supports_ssl)};
}

void boost::mysql::detail::connection_state_deleter::operator()(connection_state* st) const { delete st; }

boost::mysql::metadata_mode boost::mysql::detail::meta_mode(const connection_state& st)
{
    return st.data().meta_mode;
}

void boost::mysql::detail::set_meta_mode(connection_state& st, metadata_mode v) { st.data().meta_mode = v; }

bool boost::mysql::detail::ssl_active(const connection_state& st) { return st.data().ssl_active(); }

bool boost::mysql::detail::backslash_escapes(const connection_state& st)
{
    return st.data().backslash_escapes;
}

boost::mysql::diagnostics& boost::mysql::detail::shared_diag(connection_state& st)
{
    return st.data().shared_diag;
}

boost::system::result<boost::mysql::character_set> boost::mysql::detail::current_character_set(
    const connection_state& st
)
{
    const auto* res = st.data().charset_ptr();
    if (res == nullptr)
        return client_errc::unknown_character_set;
    return *res;
}

template <class AlgoParams>
boost::mysql::detail::any_resumable_ref boost::mysql::detail::setup(
    connection_state& st,
    const AlgoParams& params
)
{
    return st.setup(params);
}

template <class AlgoParams>
typename AlgoParams::result_type boost::mysql::detail::get_result(const connection_state& st)
{
    return st.result<AlgoParams>();
}

#ifdef BOOST_MYSQL_SEPARATE_COMPILATION

#define BOOST_MYSQL_INSTANTIATE_SETUP(op_params_type) \
    template any_resumable_ref setup<op_params_type>(connection_state&, const op_params_type&);

#define BOOST_MYSQL_INSTANTIATE_GET_RESULT(op_params_type) \
    template op_params_type::result_type get_result<op_params_type>(const connection_state&);

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
