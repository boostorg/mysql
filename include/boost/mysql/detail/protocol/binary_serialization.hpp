#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_BINARY_SERIALIZATION_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_BINARY_SERIALIZATION_HPP_

#include "boost/mysql/detail/protocol/serialization.hpp"
#include "boost/mysql/value.hpp"

// (de)serialization overloads for date/time types and mysql::value

namespace boost {
namespace mysql {
namespace detail {

inline std::size_t get_size(const date& input, const serialization_context& ctx) noexcept;
inline void serialize(const date& input, serialization_context& ctx) noexcept;
inline errc deserialize(date& output, deserialization_context& ctx) noexcept;

inline std::size_t get_size(const datetime& input, const serialization_context& ctx) noexcept;
inline void serialize(const datetime& input, serialization_context& ctx) noexcept;
inline errc deserialize(datetime& output, deserialization_context& ctx) noexcept;

inline std::size_t get_size(const time& input, const serialization_context& ctx) noexcept;
inline void serialize(const time& input, serialization_context& ctx) noexcept;
inline errc deserialize(time& output, deserialization_context& ctx) noexcept;

inline std::size_t get_size(const value& input, const serialization_context& ctx) noexcept;
inline void serialize(const value& input, serialization_context& ctx) noexcept;

} // detail
} // mysql
} // boost

#include "boost/mysql/detail/protocol/impl/binary_serialization.ipp"

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_BINARY_SERIALIZATION_HPP_ */
