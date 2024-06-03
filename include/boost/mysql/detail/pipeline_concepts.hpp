//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PIPELINE_CONCEPTS_HPP
#define BOOST_MYSQL_DETAIL_PIPELINE_CONCEPTS_HPP

#include <boost/mysql/detail/config.hpp>

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

// PipelineStageType
template <class T>
struct is_pipeline_stage_type : std::false_type
{
};

// PipelineRequestType
template <class T>
struct is_pipeline_request_type : std::false_type
{
};

#ifdef BOOST_MYSQL_HAS_CONCEPTS

// If you're getting errors pointing to this line, you're passing invalid stage types
// to a pipeline. Valid types include execute_stage, prepare_statement_stage, close_statement_stage,
// reset_connection_stage and set_character_set_stage
template <class T>
concept pipeline_stage_type = is_pipeline_stage_type<T>::value;

template <class T>
concept pipeline_request_type = is_pipeline_request_type<T>::value;

#define BOOST_MYSQL_PIPELINE_STAGE_TYPE ::boost::mysql::detail::pipeline_stage_type
#define BOOST_MYSQL_PIPELINE_REQUEST_TYPE ::boost::mysql::detail::pipeline_request_type

#else

#define BOOST_MYSQL_PIPELINE_STAGE_TYPE class
#define BOOST_MYSQL_PIPELINE_REQUEST_TYPE class

#endif

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
