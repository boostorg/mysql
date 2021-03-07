//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_VALUE_HPP
#define BOOST_MYSQL_IMPL_VALUE_HPP

#include "boost/mysql/detail/auxiliar/container_equals.hpp"
#include "boost/mysql/detail/protocol/date.hpp"
#include <limits>

namespace boost {
namespace mysql {
namespace detail {

struct print_visitor
{
    std::ostream& os;

    print_visitor(std::ostream& os): os(os) {};

    template <class T>
    void operator()(const T& value) const { os << value; }

    void operator()(const date& value) const
    {
        assert(value >= min_date && value <= max_date);
        auto ymd = days_to_ymd(value.time_since_epoch().count());
        char buffer [32] {};
        snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u", ymd.years, ymd.month, ymd.day);
        os << buffer;
    }
    void operator()(const datetime& value) const
    {
        using namespace std::chrono;
        date date_part = time_point_cast<days>(value);
        if (date_part > value)
            date_part -= days(1);
        auto tod = value - date_part;
        (*this)(date_part); // date part
        os << ' '; // separator
        (*this)(duration_cast<time>(tod)); // time of day part
    }
    void operator()(const time& value) const
    {
        using namespace std::chrono;
        assert(value >= min_time && value <= max_time);
        char buffer [64] {};
        const char* sign = value < microseconds(0) ? "-" : "";
        auto num_micros = value % seconds(1);
        auto num_secs = duration_cast<seconds>(value % minutes(1) - num_micros);
        auto num_mins = duration_cast<minutes>(value % hours(1) - num_secs);
        auto num_hours = duration_cast<hours>(value - num_mins);

        snprintf(buffer, sizeof(buffer), "%s%02d:%02u:%02u.%06u",
            sign,
            static_cast<int>(std::abs(num_hours.count())),
            static_cast<unsigned>(std::abs(num_mins.count())),
            static_cast<unsigned>(std::abs(num_secs.count())),
            static_cast<unsigned>(std::abs(num_micros.count()))
        );

        os << buffer;
    }
    void operator()(null_t) const { os << "<NULL>"; }
};

template <class T, template <class> class Optional>
struct value_converter
{
    static BOOST_CXX14_CONSTEXPR Optional<T> convert(const value::variant_type&) noexcept
    {
        return {};
    }
};

template <template <class> class Optional>
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

template <template <class> class Optional>
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

template <template <class> class Optional>
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

template <class T, template <class> class Optional>
BOOST_CXX14_CONSTEXPR Optional<T> get_optional_impl(
    const value::variant_type& val
) noexcept
{
    auto* res = boost::variant2::get_if<T>(&val);
    return res ? Optional<T>(*res) : value_converter<T, Optional>::convert(val);
}

// Helper class to implement is_convertible_to
// We can't use a boost::optional because it's not a literal type,
// and std::optional is only available in C++17
template <typename T>
class is_convertible_to_helper
{
    bool has_value_ {false};
public:
    constexpr is_convertible_to_helper() = default;
    constexpr is_convertible_to_helper(const T&) noexcept : has_value_(true) {}
    constexpr bool has_value() const noexcept { return has_value_; }
};

} // detail
} // mysql
} // boost

template <class T>
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

template <class T>
BOOST_CXX14_CONSTEXPR bool boost::mysql::value::is_convertible_to() const noexcept
{
    return detail::get_optional_impl<T, detail::is_convertible_to_helper>(repr_).has_value();
}

template <class T>
boost::optional<T> boost::mysql::value::get_optional() const noexcept
{
    return detail::get_optional_impl<T, boost::optional>(repr_);
}

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
template <class T>
constexpr std::optional<T> boost::mysql::value::get_std_optional() const noexcept
{
    return detail::get_optional_impl<T, std::optional>(repr_);
}
#endif

template <class T>
T boost::mysql::value::get() const
{
    auto res = get_optional<T>();
    if (!res)
        throw bad_value_access();
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

template <class... Types>
BOOST_CXX14_CONSTEXPR std::array<boost::mysql::value, sizeof...(Types)>
boost::mysql::make_values(
    Types&&... args
)
{
    return std::array<value, sizeof...(Types)>{value(std::forward<Types>(args))...};
}


#endif
