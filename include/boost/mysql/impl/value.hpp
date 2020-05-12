//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_VALUE_HPP
#define BOOST_MYSQL_IMPL_VALUE_HPP

#include "boost/mysql/detail/auxiliar/container_equals.hpp"

namespace boost {
namespace mysql {
namespace detail {

inline bool is_out_of_range(
    const date& d
)
{
    return d < min_date || d > max_date;
}

// Range checks
static_assert(date::min() <= min_date);
static_assert(date::max() >= max_date);
static_assert(datetime::min() <= min_datetime);
static_assert(datetime::max() >= max_datetime);
static_assert(time::min() <= min_time);
static_assert(time::max() >= max_time);

struct print_visitor
{
    std::ostream& os;

    print_visitor(std::ostream& os): os(os) {};

    template <typename T>
    void operator()(const T& value) const { os << value; }

    void operator()(const date& value) const { ::date::operator<<(os, value); }
    void operator()(const time& value) const
    {
        char buffer [100] {};
        const char* sign = value < std::chrono::microseconds(0) ? "-" : "";
        auto hours = std::abs(std::chrono::duration_cast<std::chrono::hours>(value).count());
        auto mins = std::abs(std::chrono::duration_cast<std::chrono::minutes>(value % std::chrono::hours(1)).count());
        auto secs = std::abs(std::chrono::duration_cast<std::chrono::seconds>(value % std::chrono::minutes(1)).count());
        auto micros = std::abs((value % std::chrono::seconds(1)).count());

        snprintf(buffer, sizeof(buffer), "%s%02d:%02u:%02u:%06u",
            sign,
            static_cast<int>(hours),
            static_cast<unsigned>(mins),
            static_cast<unsigned>(secs),
            static_cast<unsigned>(micros)
        );

        os << buffer;
    }
    void operator()(const datetime& value) const { ::date::operator<<(os, value); }
    void operator()(std::nullptr_t) const { os << "<NULL>"; }
};

} // detail
} // mysql
} // boost

inline bool boost::mysql::operator==(
    const value& lhs,
    const value& rhs
)
{
    if (lhs.index() != rhs.index())
        return false;
    return std::visit([&lhs](const auto& rhs_value) {
        using T = std::decay_t<decltype(rhs_value)>;
        return std::get<T>(lhs) == rhs_value;
    }, rhs);
}

inline bool boost::mysql::operator==(
    const std::vector<value>& lhs,
    const std::vector<value>& rhs
)
{
    return detail::container_equals(lhs, rhs);
}

inline std::ostream& boost::mysql::operator<<(
    std::ostream& os,
    const value& value
)
{
    std::visit(detail::print_visitor(os), value);
    return os;
}

template <typename... Types>
std::array<boost::mysql::value, sizeof...(Types)>
boost::mysql::make_values(
    Types&&... args
)
{
    return std::array<value, sizeof...(Types)>{value(std::forward<Types>(args))...};
}


#endif
