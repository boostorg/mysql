//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_STATE_API_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_STATE_API_HPP

// Visible API for connection_state

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

class connection_state_api
{
    struct pimpl_deleter
    {
        BOOST_MYSQL_DECL void operator()(connection_state*) const noexcept;
    };

    std::unique_ptr<connection_state, pimpl_deleter> st_;

public:
    BOOST_MYSQL_DECL connection_state_api(std::size_t read_buff_size, bool stream_supports_ssl);

    // Getters/setters
    BOOST_MYSQL_DECL std::vector<field_view>& get_shared_fields() noexcept;
    BOOST_MYSQL_DECL diagnostics& shared_diag() noexcept;
    BOOST_MYSQL_DECL metadata_mode meta_mode() const noexcept;
    BOOST_MYSQL_DECL void set_meta_mode(metadata_mode) noexcept;
    BOOST_MYSQL_DECL bool ssl_active() const noexcept;
    BOOST_MYSQL_DECL bool backslash_escapes() const noexcept;
    BOOST_MYSQL_DECL system::result<character_set> current_character_set() const noexcept;

    // Running algorithms
    template <class AlgoParams>
    any_resumable_ref setup(const AlgoParams&);

    // Note: AlgoParams should have !is_void_result
    template <class AlgoParams>
    typename AlgoParams::result_type get_result() const noexcept;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/connection_state_api.ipp>
#endif

#endif
