#ifndef INCLUDE_MYSQL_IMPL_BINARY_SERIALIZATION_HPP_
#define INCLUDE_MYSQL_IMPL_BINARY_SERIALIZATION_HPP_

#include "mysql/impl/serialization.hpp"
#include "mysql/value.hpp"

// (de)serialization overloads for date/time types and mysql::value

namespace mysql
{
namespace detail
{

inline std::size_t get_size(const date& input, const SerializationContext& ctx) noexcept;
inline void serialize(const date& input, SerializationContext& ctx) noexcept;
inline Error deserialize(date& output, DeserializationContext& ctx) noexcept;

inline std::size_t get_size(const datetime& input, const SerializationContext& ctx) noexcept;
inline void serialize(const datetime& input, SerializationContext& ctx) noexcept;
inline Error deserialize(datetime& output, DeserializationContext& ctx) noexcept;

inline std::size_t get_size(const time& input, const SerializationContext& ctx) noexcept;
inline void serialize(const time& input, SerializationContext& ctx) noexcept;
inline Error deserialize(time& output, DeserializationContext& ctx) noexcept;

inline std::size_t get_size(const value& input, const SerializationContext& ctx) noexcept;
inline void serialize(const value& input, SerializationContext& ctx) noexcept;

}
}

#include "mysql/impl/binary_serialization.ipp"

#endif /* INCLUDE_MYSQL_IMPL_BINARY_SERIALIZATION_HPP_ */
