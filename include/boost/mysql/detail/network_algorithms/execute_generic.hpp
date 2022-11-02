//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_GENERIC_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_EXECUTE_GENERIC_HPP

#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/protocol/query_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/resultset_base.hpp>

#include <type_traits>
#include <utility>

#ifdef BOOST_MYSQL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace mysql {
namespace detail {

#ifdef BOOST_MYSQL_HAS_CONCEPTS
// clang-format off
template <class T>
struct is_com_stmt_execute_packet : std::false_type {};

template <class FieldViewFwdIterator>
struct is_com_stmt_execute_packet<com_stmt_execute_packet<FieldViewFwdIterator>> : std::true_type {};

template <class T>
concept execute_request = std::is_same_v<T, com_query_packet> || is_com_stmt_execute_packet<T>::value;

template <class T>
concept execute_request_maker = requires(const T& t)
{
    typename T::storage_type; // Used by prepared statements (tuple version) to store field_view's
    requires std::default_initializable<typename T::storage_type>;
    requires std::copy_constructible<T>;
    { t.make_storage() } -> std::same_as<typename T::storage_type>;
    { t.make_request(std::declval<const typename T::storage_type&>()) } -> execute_request;
};
// clang-format on

#define BOOST_MYSQL_EXECUTE_REQUEST ::boost::mysql::detail::execute_request
#define BOOST_MYSQL_EXECUTE_REQUEST_MAKER ::boost::mysql::detail::execute_request_maker

#else  // BOOST_MYSQL_HAS_CONCEPTS

#define BOOST_MYSQL_EXECUTE_REQUEST class
#define BOOST_MYSQL_EXECUTE_REQUEST_MAKER class

#endif  // BOOST_MYSQL_HAS_CONCEPTS

// The sync version gets passed directlty the request packet to be serialized.
// There is no need to defer the serialization here.
template <class Stream, BOOST_MYSQL_EXECUTE_REQUEST Request>
void execute_generic(
    resultset_encoding encoding,
    channel<Stream>& channel,
    const Request& request,
    resultset_base& output,
    error_code& err,
    error_info& info
);

// The async version gets passed a request maker, holding enough data to create
// the request packet when the async op is started. Used by statement execution
// with tuple params, to make lifetimes more user-friendly when using deferred tokens.
template <class Stream, BOOST_MYSQL_EXECUTE_REQUEST_MAKER RequestMaker, class CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
async_execute_generic(
    resultset_encoding encoding,
    channel<Stream>& chan,
    RequestMaker&& reqmaker,  // this should always by an rvalue
    resultset_base& output,
    error_info& info,
    CompletionToken&& token
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/network_algorithms/impl/execute_generic.hpp>

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_ */
