//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_STATE_API_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_STATE_API_HPP

// Visible API for connection_state. Having this hides connection_state
// dependencies from consumers in non-header-only builds

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/any_resumable_ref.hpp>
#include <boost/mysql/detail/config.hpp>

#include <boost/system/result.hpp>

#include <memory>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

class connection_state;

// Destruction
struct connection_state_deleter
{
    BOOST_MYSQL_DECL void operator()(connection_state*) const;
};
using connection_state_ptr = std::unique_ptr<connection_state, connection_state_deleter>;

// Construction
BOOST_MYSQL_DECL connection_state_ptr
create_connection_state(std::size_t read_buff_size, bool stream_supports_ssl);

// Getters/setters
BOOST_MYSQL_DECL std::vector<field_view>& get_shared_fields(connection_state&);
BOOST_MYSQL_DECL diagnostics& shared_diag(connection_state&);
BOOST_MYSQL_DECL metadata_mode meta_mode(const connection_state&);
BOOST_MYSQL_DECL void set_meta_mode(connection_state&, metadata_mode);
BOOST_MYSQL_DECL bool ssl_active(const connection_state&);
BOOST_MYSQL_DECL bool backslash_escapes(const connection_state&);
BOOST_MYSQL_DECL system::result<character_set> current_character_set(const connection_state&);

// Running algorithms
template <class AlgoParams>
any_resumable_ref setup(connection_state&, const AlgoParams&);

// Note: AlgoParams should have !is_void_result
template <class AlgoParams>
typename AlgoParams::result_type get_result(const connection_state&);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/connection_state_api.ipp>
#endif

#endif
