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
#ifndef BOOST_NO_CXX14_CONSTEXPR
static_assert(date::min() <= min_date, "Range check failed");
static_assert(date::max() >= max_date, "Range check failed");
static_assert(datetime::min() <= min_datetime, "Range check failed");
static_assert(datetime::max() >= max_datetime, "Range check failed");
static_assert(time::min() <= min_time, "Range check failed");
static_assert(time::max() >= max_time, "Range check failed");
#endif

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

        snprintf(buffer, sizeof(buffer), "%s%02d:%02u:%02u.%06u",
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

template <typename T, template <typename> class Optional>
struct value_converter
{
    static BOOST_CXX14_CONSTEXPR Optional<T> convert(const value::variant_type&) noexcept
    {
        return {};
    }
};

template <template <typename> class Optional>
struct value_converter<std::uint64_t, Optional>
{
    static BOOST_CXX14_CONSTEXPR Optional<std::uint64_t> convert(
        const value::variant_type& repr
    ) noexcept
    {
        auto* val = boost::variant2::get_if<std::int64_t>(&repr);
        return (val && *val >= 0) ?
            Optional<std::uint64_t>(static_cast<std::uint64_t>(*val)) :
            Optional<std::uint64_t>();
    }
};

template <template <typename> class Optional>
struct value_converter<std::int64_t, Optional>
{
    static BOOST_CXX14_CONSTEXPR Optional<std::int64_t> convert(
        const value::variant_type& repr
    ) noexcept
    {
        auto* val = boost::variant2::get_if<std::uint64_t>(&repr);
        return (val && *val <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) ?
            Optional<std::int64_t>(static_cast<std::int64_t>(*val)) :
            Optional<std::int64_t>();
    }
};

template <template <typename> class Optional>
struct value_converter<double, Optional>
{
    static BOOST_CXX14_CONSTEXPR Optional<double> convert(
        const value::variant_type& repr
    ) noexcept
    {
        auto* val = boost::variant2::get_if<float>(&repr);
        return val ? Optional<double>(*val) : Optional<double>();
    }
};

template <typename T, template <typename> class Optional>
BOOST_CXX14_CONSTEXPR Optional<T> get_optional_impl(
    const value::variant_type& val
) noexcept
{
    auto* res = boost::variant2::get_if<T>(&val);
    return res ? Optional<T>(*res) : value_converter<T, Optional>::convert(val);
}

} // detail
} // mysql
} // boost

template <typename T>
BOOST_CXX14_CONSTEXPR boost::mysql::value::value(
    const T& v
) noexcept :
    value(
        v,
        typename std::conditional<
            std::is_unsigned<T>::value,
            unsigned_int_tag,
            typename std::conditional<
                std::is_integral<T>::value && std::is_signed<T>::value,
                signed_int_tag,
                no_tag
            >::type
        >::type()
    )
{
}

template <typename T>
boost::optional<T> boost::mysql::value::get_optional() const noexcept
{
    return detail::get_optional_impl<T, boost::optional>(repr_);
}

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
template <typename T>
constexpr std::optional<T> boost::mysql::value::get_std_optional() const noexcept
{
    return detail::get_optional_impl<T, std::optional>(repr_);
}
#endif

template <typename T>
T boost::mysql::value::get() const
{
    auto res = get_optional<T>();
    if (!res)
        throw boost::variant2::bad_variant_access();
    return *res;
}

inline std::ostream& boost::mysql::operator<<(
    std::ostream& os,
    const value& value
)
{
    boost::variant2::visit(detail::print_visitor(os), value.to_variant());
    return os;
}

template <typename... Types>
BOOST_CXX14_CONSTEXPR std::array<boost::mysql::value, sizeof...(Types)>
boost::mysql::make_values(
    Types&&... args
)
{
    return std::array<value, sizeof...(Types)>{value(std::forward<Types>(args))...};
}


#endif
