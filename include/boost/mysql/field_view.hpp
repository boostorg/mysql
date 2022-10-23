//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FIELD_VIEW_HPP
#define BOOST_MYSQL_FIELD_VIEW_HPP

#include <boost/mysql/datetime_types.hpp>
#include <boost/mysql/detail/auxiliar/field_impl.hpp>
#include <boost/mysql/detail/auxiliar/string_view_offset.hpp>
#include <boost/mysql/field_kind.hpp>

#include <boost/config/detail/suffix.hpp>
#include <boost/utility/string_view.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace boost {
namespace mysql {

/**
 * \brief Represents a value in the database of any of the allowed types.
 *        See [link mysql.values this section] for more info.
 * \details
 *
 * A [reflink value] is a variant-like class. At a given time,
 * it always holds a value of one of the type alternatives.
 * See [link mysql.values this section] for information
 * on how to use this class.
 *
 * This is a lightweight, cheap-to-copy class. Strings
 * are represented as string_views, not as owning strings.
 * This implies that if a value is a string, it will point
 * to a [*externally owned] piece of memory. See
 * [link mysql.values.strings this section] for more info.
 */
class field_view
{
public:
    /// Constructs a NULL value.
    BOOST_CXX14_CONSTEXPR field_view() = default;

    /**
     * \brief Constructs a NULL value.
     * \details
     * Caution: `value(NULL)` will __NOT__ match this overload. It will try to construct
     * a `boost::string_view` from a NULL C string, causing undefined behavior.
     */
    BOOST_CXX14_CONSTEXPR explicit field_view(std::nullptr_t) noexcept {}

    BOOST_CXX14_CONSTEXPR explicit inline field_view(signed char v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(short v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(int v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(long v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(long long v) noexcept;

    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned char v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned short v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned int v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned long v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned long long v) noexcept;

    BOOST_CXX14_CONSTEXPR explicit inline field_view(boost::string_view v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(float v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(double v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(const date& v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(const datetime& v) noexcept;
    BOOST_CXX14_CONSTEXPR explicit inline field_view(const time& v) noexcept;

    // TODO: hide this
    BOOST_CXX14_CONSTEXPR explicit field_view(detail::string_view_offset v) noexcept
        : ikind_(internal_kind::sv_offset)
    {
        repr_.sv_offset = {v.offset(), v.size()};
    }
    BOOST_CXX14_CONSTEXPR explicit field_view(const detail::field_impl* v) noexcept
        : ikind_(internal_kind::field_ptr)
    {
        repr_.field_ptr = v;
    }

    BOOST_CXX14_CONSTEXPR inline field_kind kind() const noexcept;

    BOOST_CXX14_CONSTEXPR bool is_null() const noexcept { return kind() == field_kind::null; }
    BOOST_CXX14_CONSTEXPR bool is_int64() const noexcept { return kind() == field_kind::int64; }
    BOOST_CXX14_CONSTEXPR bool is_uint64() const noexcept { return kind() == field_kind::uint64; }
    BOOST_CXX14_CONSTEXPR bool is_string() const noexcept { return kind() == field_kind::string; }
    BOOST_CXX14_CONSTEXPR bool is_float() const noexcept { return kind() == field_kind::float_; }
    BOOST_CXX14_CONSTEXPR bool is_double() const noexcept { return kind() == field_kind::double_; }
    BOOST_CXX14_CONSTEXPR bool is_date() const noexcept { return kind() == field_kind::date; }
    BOOST_CXX14_CONSTEXPR bool is_datetime() const noexcept
    {
        return kind() == field_kind::datetime;
    }
    BOOST_CXX14_CONSTEXPR bool is_time() const noexcept { return kind() == field_kind::time; }

    BOOST_CXX14_CONSTEXPR inline std::int64_t as_int64() const;
    BOOST_CXX14_CONSTEXPR inline std::uint64_t as_uint64() const;
    BOOST_CXX14_CONSTEXPR inline boost::string_view as_string() const;
    BOOST_CXX14_CONSTEXPR inline float as_float() const;
    BOOST_CXX14_CONSTEXPR inline double as_double() const;
    BOOST_CXX14_CONSTEXPR inline date as_date() const;
    BOOST_CXX14_CONSTEXPR inline datetime as_datetime() const;
    BOOST_CXX14_CONSTEXPR inline time as_time() const;

    BOOST_CXX14_CONSTEXPR inline std::int64_t get_int64() const noexcept;
    BOOST_CXX14_CONSTEXPR inline std::uint64_t get_uint64() const noexcept;
    BOOST_CXX14_CONSTEXPR inline boost::string_view get_string() const noexcept;
    BOOST_CXX14_CONSTEXPR inline float get_float() const noexcept;
    BOOST_CXX14_CONSTEXPR inline double get_double() const noexcept;
    BOOST_CXX14_CONSTEXPR inline date get_date() const noexcept;
    BOOST_CXX14_CONSTEXPR inline datetime get_datetime() const noexcept;
    BOOST_CXX14_CONSTEXPR inline time get_time() const noexcept;

    /// Tests for equality (type and value); see [link mysql.values.relational this section] for
    /// more info.
    BOOST_CXX14_CONSTEXPR inline bool operator==(const field_view& rhs) const noexcept;

    /// Tests for inequality (type and value); see [link mysql.values.relational this section] for
    /// more info.
    BOOST_CXX14_CONSTEXPR bool operator!=(const field_view& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    // TODO: hide this
    void offset_to_string_view(const std::uint8_t* buffer_first) noexcept
    {
        if (ikind_ == internal_kind::sv_offset)
        {
            ikind_ = internal_kind::string;
            repr_.string = {
                reinterpret_cast<const char*>(buffer_first) + repr_.sv_offset.offset,
                repr_.sv_offset.size};
        }
    }

private:
    enum class internal_kind
    {
        null = 0,
        int64,
        uint64,
        string,
        float_,
        double_,
        date,
        datetime,
        time,
        sv_offset,
        field_ptr
    };

    union repr_t
    {
        std::int64_t int64;
        std::uint64_t uint64;
        struct
        {
            const char* ptr;
            std::size_t size;
        } string;
        float float_;
        double double_;
        date::rep date_;
        datetime::rep datetime_;
        time::rep time_;
        struct
        {
            std::size_t offset;
            std::size_t size;
        } sv_offset;
        const detail::field_impl* field_ptr;

        BOOST_CXX14_CONSTEXPR repr_t() noexcept : int64{} {}
        BOOST_CXX14_CONSTEXPR repr_t(std::int64_t v) noexcept : int64(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(std::uint64_t v) noexcept : uint64(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(boost::string_view v) noexcept : string{v.data(), v.size()} {}
        BOOST_CXX14_CONSTEXPR repr_t(float v) noexcept : float_(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(double v) noexcept : double_(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(date v) noexcept : date_(v.time_since_epoch().count()) {}
        BOOST_CXX14_CONSTEXPR repr_t(datetime v) noexcept : datetime_(v.time_since_epoch().count())
        {
        }
        BOOST_CXX14_CONSTEXPR repr_t(time v) noexcept : time_(v.count()) {}
        BOOST_CXX14_CONSTEXPR repr_t(detail::string_view_offset v) noexcept
            : sv_offset{v.offset(), v.size()}
        {
        }
        BOOST_CXX14_CONSTEXPR repr_t(const detail::field_impl* v) noexcept : field_ptr(v) {}

        BOOST_CXX14_CONSTEXPR boost::string_view get_string() const noexcept
        {
            return boost::string_view(string.ptr, string.size);
        }
        BOOST_CXX14_CONSTEXPR date get_date() const noexcept { return date(date::duration(date_)); }
        BOOST_CXX14_CONSTEXPR datetime get_datetime() const noexcept
        {
            return datetime(datetime::duration(datetime_));
        }
        BOOST_CXX14_CONSTEXPR time get_time() const noexcept { return time(time_); }
        BOOST_CXX14_CONSTEXPR detail::string_view_offset get_sv_offset() const noexcept
        {
            return detail::string_view_offset(sv_offset.offset, sv_offset.size);
        }
    };

    internal_kind ikind_{internal_kind::null};
    repr_t repr_;

    friend std::ostream& operator<<(std::ostream& os, const field_view& v);

    BOOST_CXX14_CONSTEXPR bool is_field_ptr() const noexcept
    {
        return ikind_ == internal_kind::field_ptr;
    }
    BOOST_CXX14_CONSTEXPR inline void check_kind(internal_kind expected) const;
};

/**
 * \relates value
 * \brief Streams a value.
 * \details The value should be in the MySQL valid range of values. Concretely,
 * if the value is a [reflink date], [reflink datetime] or
 * [reflink time], it should be in the
 * \\[[reflink min_date], [reflink max_date]\\],
 * \\[[reflink min_datetime], [reflink max_datetime]\\] or
 * \\[[reflink min_time], [reflink max_time]\\], respectively.
 * Otherwise, the results are undefined.
 */
inline std::ostream& operator<<(std::ostream& os, const field_view& v);

/**
 * \relates value
 * \brief Creates an array of [reflink value]s out of the passed in arguments.
 * \details Each argument creates an element in the array. It should be possible
 * to construct a [reflink value] out of every single argument passed in.
 */
template <class... Types>
BOOST_CXX14_CONSTEXPR std::array<field_view, sizeof...(Types)> make_field_views(Types&&... args);

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/field_view.hpp>

#endif
