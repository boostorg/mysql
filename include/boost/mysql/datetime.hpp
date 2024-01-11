//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DATETIME_HPP
#define BOOST_MYSQL_DATETIME_HPP

#include <boost/mysql/days.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/datetime.hpp>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/core/span.hpp>
#include <boost/throw_exception.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <ratio>
#include <stdexcept>

namespace boost {
namespace mysql {

/**
 * \brief Type representing MySQL `DATETIME` and `TIMESTAMP` data types.
 * \details Represents a broken datetime by its year, month, day, hour, minute, second and
 * microsecond components. This type is close to the protocol and should not be used as a vocabulary
 * type. Instead, cast it to a `std::chrono::time_point` by calling \ref as_time_point or \ref
 * get_time_point.
 * \n
 * As opposed to `time_point`, this type allows representing invalid and zero datetimes.
 */
class datetime
{
public:
    /**
     * \brief A `std::chrono::time_point` that can represent any valid datetime.
     * \details Represents microseconds since the UNIX epoch, with the same precision for all architectures.
     */
    using time_point = std::chrono::
        time_point<std::chrono::system_clock, std::chrono::duration<std::int64_t, std::micro>>;

    /**
     * \brief Constructs a zero datetime.
     * \details Results in a datetime with all of its components set to zero.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr datetime() noexcept = default;

    /**
     * \brief Constructs a datetime from its individual components.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr datetime(
        std::uint16_t year,
        std::uint8_t month,
        std::uint8_t day,
        std::uint8_t hour = 0,
        std::uint8_t minute = 0,
        std::uint8_t second = 0,
        std::uint32_t microsecond = 0
    ) noexcept
        : impl_{year, month, day, hour, minute, second, microsecond}
    {
    }

    /**
     * \brief Constructs a datetime from a `time_point`.
     * \par Exception safety
     * Strong guarantee. Throws on invalid input.
     * \throws std::out_of_range If the resulting `datetime` object would be
     * out of the [\ref min_datetime, \ref max_datetime] range.
     */
    BOOST_CXX14_CONSTEXPR inline explicit datetime(time_point tp);

    /**
     * \brief Retrieves the year component.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint16_t year() const noexcept { return impl_.year; }

    /**
     * \brief Retrieves the month component.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t month() const noexcept { return impl_.month; }

    /**
     * \brief Retrieves the day component.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t day() const noexcept { return impl_.day; }

    /**
     * \brief Retrieves the hour component.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t hour() const noexcept { return impl_.hour; }

    /**
     * \brief Retrieves the minute component.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t minute() const noexcept { return impl_.minute; }

    /**
     * \brief Retrieves the second component.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t second() const noexcept { return impl_.second; }

    /**
     * \brief Retrieves the microsecond component.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint32_t microsecond() const noexcept { return impl_.microsecond; }

    /**
     * \brief Returns `true` if `*this` represents a valid `time_point`.
     * \details If any of the individual components is out of range, the datetime
     * doesn't represent an actual `time_point` (e.g. `datetime(2020, 2, 30)`) or
     * the datetime is not in the [\ref min_date, \ref max_date] validity range,
     * returns `false`. Otherwise, returns `true`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr bool valid() const noexcept
    {
        return detail::is_valid(impl_.year, impl_.month, impl_.day) && impl_.hour <= detail::max_hour &&
               impl_.minute <= detail::max_min && impl_.second <= detail::max_sec &&
               impl_.microsecond <= detail::max_micro;
    }

    /**
     * \brief Converts `*this` into a `time_point` (unchecked access).
     * \par Preconditions
     * `this->valid() == true` (if violated, results in undefined behavior).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    BOOST_CXX14_CONSTEXPR time_point get_time_point() const noexcept
    {
        BOOST_ASSERT(valid());
        return unch_get_time_point();
    }

    /**
     * \brief Converts `*this` into a `time_point` (checked access).
     * \par Exception safety
     * Strong guarantee.
     * \throws std::invalid_argument If `!this->valid()`.
     */
    BOOST_CXX14_CONSTEXPR inline time_point as_time_point() const
    {
        if (!valid())
            BOOST_THROW_EXCEPTION(std::invalid_argument("datetime::as_time_point: invalid datetime"));
        return unch_get_time_point();
    }

    /**
     * \brief Tests for equality.
     * \details Two datetimes are considered equal if all of its individual components
     * are equal. This function works for invalid datetimes, too.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr bool operator==(const datetime& rhs) const noexcept
    {
        return impl_.year == rhs.impl_.year && impl_.month == rhs.impl_.month && impl_.day == rhs.impl_.day &&
               impl_.hour == rhs.impl_.hour && impl_.minute == rhs.impl_.minute &&
               impl_.second == rhs.impl_.second && impl_.microsecond == rhs.impl_.microsecond;
    }

    /**
     * \brief Tests for inequality.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr bool operator!=(const datetime& rhs) const noexcept { return !(*this == rhs); }

    /**
     * \brief Returns the current system time as a datetime object.
     * \par Exception safety
     * Strong guarantee. Only throws if obtaining the current time throws.
     */
    static datetime now()
    {
        auto now = time_point::clock::now();
        return datetime(std::chrono::time_point_cast<time_point::duration>(now));
    }

private:
    struct impl_t
    {
        std::uint16_t year;
        std::uint8_t month;
        std::uint8_t day;
        std::uint8_t hour;
        std::uint8_t minute;
        std::uint8_t second;
        std::uint32_t microsecond;

        BOOST_MYSQL_DECL
        std::size_t to_string(span<char, 64> output) const noexcept;
    } impl_{};

    friend struct detail::access;

    BOOST_CXX14_CONSTEXPR inline time_point unch_get_time_point() const noexcept
    {
        // Doing time of day independently to prevent overflow
        days d(detail::ymd_to_days(impl_.year, impl_.month, impl_.day));
        auto time_of_day = std::chrono::hours(impl_.hour) + std::chrono::minutes(impl_.minute) +
                           std::chrono::seconds(impl_.second) + std::chrono::microseconds(impl_.microsecond);
        return time_point(d) + time_of_day;
    }
};

/**
 * \relates datetime
 * \brief Streams a datetime.
 * \details This function works for invalid datetimes, too.
 */
BOOST_MYSQL_DECL
std::ostream& operator<<(std::ostream& os, const datetime& v);

/// The minimum allowed value for \ref datetime.
constexpr datetime min_datetime(0u, 1u, 1u);

/// The maximum allowed value for \ref datetime.
constexpr datetime max_datetime(9999u, 12u, 31u, 23u, 59u, 59u, 999999u);

}  // namespace mysql
}  // namespace boost

// Implementations
BOOST_CXX14_CONSTEXPR boost::mysql::datetime::datetime(time_point tp)
{
    using std::chrono::duration_cast;
    using std::chrono::hours;
    using std::chrono::microseconds;
    using std::chrono::minutes;
    using std::chrono::seconds;

    // Avoiding using -= for durations as it's not constexpr until C++17
    auto input_dur = tp.time_since_epoch();
    auto rem = input_dur % days(1);
    auto num_days = duration_cast<days>(input_dur);
    if (rem.count() < 0)
    {
        rem = rem + days(1);
        num_days = num_days - days(1);
    }
    auto num_hours = duration_cast<hours>(rem);
    rem = rem - num_hours;
    auto num_minutes = duration_cast<minutes>(rem);
    rem = rem - num_minutes;
    auto num_seconds = duration_cast<seconds>(rem);
    rem = rem - num_seconds;
    auto num_microseconds = duration_cast<microseconds>(rem);

    BOOST_ASSERT(num_hours.count() >= 0 && num_hours.count() <= detail::max_hour);
    BOOST_ASSERT(num_minutes.count() >= 0 && num_minutes.count() <= detail::max_min);
    BOOST_ASSERT(num_seconds.count() >= 0 && num_seconds.count() <= detail::max_sec);
    BOOST_ASSERT(num_microseconds.count() >= 0 && num_microseconds.count() <= detail::max_micro);

    bool ok = detail::days_to_ymd(num_days.count(), impl_.year, impl_.month, impl_.day);
    if (!ok)
        BOOST_THROW_EXCEPTION(std::out_of_range("datetime::datetime: time_point was out of range"));

    impl_.microsecond = static_cast<std::uint32_t>(num_microseconds.count());
    impl_.second = static_cast<std::uint8_t>(num_seconds.count());
    impl_.minute = static_cast<std::uint8_t>(num_minutes.count());
    impl_.hour = static_cast<std::uint8_t>(num_hours.count());
}

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/datetime.ipp>
#endif

#endif
