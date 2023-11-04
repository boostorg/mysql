//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATE_DATA_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CONNECTION_STATE_DATA_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>
#include <boost/mysql/impl/internal/sansio/message_reader.hpp>
#include <boost/mysql/impl/internal/sansio/message_writer.hpp>

#include <cstddef>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

enum class ssl_state
{
    unsupported,
    inactive,
    active,
    torn_down,
};

struct connection_state_data
{
    bool is_connected{false};
    db_flavor flavor{db_flavor::mysql};
    capabilities current_capabilities;
    diagnostics shared_diag;  // for async ops
    std::vector<field_view> shared_fields;
    metadata_mode meta_mode{metadata_mode::minimal};
    message_reader reader;
    message_writer writer;
    ssl_state ssl;

    bool ssl_active() const noexcept { return ssl == ssl_state::active; }
    bool supports_ssl() const noexcept { return ssl != ssl_state::unsupported; }

    connection_state_data(std::size_t read_buffer_size, bool transport_supports_ssl = false)
        : reader(read_buffer_size), ssl(transport_supports_ssl ? ssl_state::inactive : ssl_state::unsupported)
    {
    }

    void reset()
    {
        is_connected = false;
        flavor = db_flavor::mysql;
        current_capabilities = capabilities();
        // Metadata mode does not get reset on handshake
        reader.reset();
        // Writer does not need reset, since every write clears previous state
        if (supports_ssl())
            ssl = ssl_state::inactive;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
