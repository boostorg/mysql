//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_IMPL_EXECUTION_PROCESSOR_IPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_IMPL_EXECUTION_PROCESSOR_IPP

#pragma once

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>

boost::mysql::error_code boost::mysql::detail::execution_processor::on_meta(
    const column_definition_packet& pack,
    diagnostics& diag
)
{
    return on_meta_helper(
        impl_access::construct<metadata>(pack, mode_ == metadata_mode::full),
        pack.name.value,
        diag
    );
}

void boost::mysql::detail::execution_processor::set_state_for_ok(const ok_packet& pack) noexcept
{
    if (pack.status_flags & SERVER_MORE_RESULTS_EXISTS)
    {
        set_state(state_t::reading_first_subseq);
    }
    else
    {
        set_state(state_t::complete);
    }
}

#endif
