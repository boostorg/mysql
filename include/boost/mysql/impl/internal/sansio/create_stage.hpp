//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CREATE_STAGE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CREATE_STAGE_HPP

#include <boost/mysql/detail/pipeline.hpp>

#include <boost/mysql/impl/internal/protocol/serialization.hpp>

namespace boost {
namespace mysql {
namespace detail {

// Helper to create a pipeline stage from a serialize_top_level_result
inline pipeline_request_stage create_stage(
    pipeline_stage_kind kind,
    serialize_top_level_result serialize_result,
    pipeline_request_stage::stage_specific_t stage_specific
)
{
    return serialize_result.err ? pipeline_request_stage{pipeline_stage_kind::error, 0u, serialize_result.err}
                                : pipeline_request_stage{kind, serialize_result.seqnum, stage_specific};
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
