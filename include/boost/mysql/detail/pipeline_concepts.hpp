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

#ifdef BOOST_MYSQL_HAS_CONCEPTS

#include <concepts>

namespace boost {
namespace mysql {

class pipeline_request;

template <class... PipelineStageType>
class static_pipeline_request;

namespace detail {

template <class T>
struct is_static_pipeline_request : std::false_type
{
};

template <class... T>
struct is_static_pipeline_request<static_pipeline_request<T...>> : std::true_type
{
};

template <class T>
concept pipeline_request_type = std::same_as<T, pipeline_request> || is_static_pipeline_request<T>::value;

#define BOOST_MYSQL_PIPELINE_REQUEST_TYPE ::boost::mysql::detail::pipeline_request_type

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#else

#define BOOST_MYSQL_PIPELINE_REQUEST_TYPE class

#endif

#endif
