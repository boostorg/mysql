//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_FIELD_VIEW_HPP
#define BOOST_MYSQL_FIELD_VIEW_HPP

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/time.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/auxiliar/field_impl.hpp>
#include <boost/mysql/detail/auxiliar/string_view_offset.hpp>

#include <boost/config.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace boost {
namespace mysql {

/**
 * \brief Non-owning variant-like class that can represent of any of the allowed database types.
 * \details
 * This is a variant-like class, similar to \ref field, but semi-owning and read-only. Values
 * of this type are usually created by the library, not directly by the user. It's cheap to
 * construct and copy, and it's the main library interface when reading values from MySQL.
 * \n
 * Like a variant, at any point, a `field_view` always points to a value of
 * certain type. You can query the type using \ref field_view::kind and the `is_xxx` functions
 * like \ref field_view::is_int64. Use `as_xxx` and `get_xxx` for checked and unchecked value
 * access, respectively. As opposed to \ref field, these functions return values instead of
 * references.
 * \n
 * Depending on how it was constructed, `field_view` can have value or reference semantics:
 *  - If it was created by the library, the `field_view` will have an associated \ref row or
 *    \ref rows object holding memory to which the `field_view` points. It will be valid as
 *    long as the memory allocated by that object is valid.
 *  - If it was created from a \ref field (by calling \ref field operator field_view), the
 *   `field_view` acts as a reference to that `field` object, and will be valid as long as the
 *   `field` is.
 *  - If it was created from a scalar (null, integral, floating point or date/time), the
 *    `field_view` has value semnatics and will always be valid.
 *  - If it was created from a string type, the `field_view` acts as a `string_view` type, and will
 *    be valid as long as the original string is.
 * \n
 * Calling any member function on a `field_view` that has been invalidated results in undefined
 * behavior.
 */
class field_view
{
public:
    /// \brief Constructs a `field_view` holding NULL (`this->kind() == field_kind::null`).
    /// \details Results in a `field_view` with value semantics (always valid).
    BOOST_CXX14_CONSTEXPR field_view() = default;

    /**
     * \brief (EXPERIMENTAL) Constructs a `field_view` holding NULL (`this->kind() ==
     * field_kind::null`). \details Results in a `field_view` with value semantics (always valid).
     *
     * Caution: `field_view(NULL)` will __NOT__ match this overload. It will try to construct
     * a `string_view` from a NULL C string, causing undefined behavior.
     */
    BOOST_CXX14_CONSTEXPR explicit field_view(std::nullptr_t) noexcept {}

    /// \brief (EXPERIMENTAL) Constructs a `field_view` holding an `int64` (`this->kind() ==
    /// field_kind::int64`). \details Results in a `field_view` with value semantics (always valid).
    BOOST_CXX14_CONSTEXPR explicit inline field_view(signed char v) noexcept;

    /// \copydoc field_view(signed char)
    BOOST_CXX14_CONSTEXPR explicit inline field_view(short v) noexcept;

    /// \copydoc field_view(signed char)
    BOOST_CXX14_CONSTEXPR explicit inline field_view(int v) noexcept;

    /// \copydoc field_view(signed char)
    BOOST_CXX14_CONSTEXPR explicit inline field_view(long v) noexcept;

    /// \copydoc field_view(signed char)
    BOOST_CXX14_CONSTEXPR explicit inline field_view(long long v) noexcept;

    /// \brief (EXPERIMENTAL) Constructs a `field_view` holding a `uint64` (`this->kind() ==
    /// field_kind::uint64`). \details Results in a `field_view` with value semantics (always
    /// valid).
    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned char v) noexcept;

    /// \copydoc field_view(unsigned char)
    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned short v) noexcept;

    /// \copydoc field_view(unsigned char)
    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned int v) noexcept;

    /// \copydoc field_view(unsigned char)
    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned long v) noexcept;

    /// \copydoc field_view(unsigned char)
    BOOST_CXX14_CONSTEXPR explicit inline field_view(unsigned long long v) noexcept;

    /// \brief (EXPERIMENTAL) Constructs a `field_view` holding a string (`this->kind() ==
    /// field_kind::string`). \details Results in a `field_view` with reference semantics. It will
    /// be valid as long as the character buffer the `string_view` points to is valid.
    BOOST_CXX14_CONSTEXPR explicit inline field_view(string_view v) noexcept;

    /// \brief (EXPERIMENTAL) Constructs a `field_view` holding a blob (`this->kind() ==
    /// field_kind::blob`). \details Results in a `field_view` with reference semantics. It will
    /// be valid as long as the buffer the `blob_view` points to is valid.
    BOOST_CXX14_CONSTEXPR explicit inline field_view(blob_view v) noexcept;

    /// \brief (EXPERIMENTAL) Constructs a `field_view` holding a float (`this->kind() ==
    /// field_kind::float_`). \details Results in a `field_view` with value semantics (always
    /// valid).
    BOOST_CXX14_CONSTEXPR explicit inline field_view(float v) noexcept;

    /// \brief (EXPERIMENTAL) Constructs a `field_view` holding a double (`this->kind() ==
    /// field_kind::double_`). \details Results in a `field_view` with value semantics (always
    /// valid).
    BOOST_CXX14_CONSTEXPR explicit inline field_view(double v) noexcept;

    /// \brief (EXPERIMENTAL) Constructs a `field_view` holding a date (`this->kind() ==
    /// field_kind::date`). \details Results in a `field_view` with value semantics (always valid).
    BOOST_CXX14_CONSTEXPR explicit inline field_view(const date& v) noexcept;

    /// \brief (EXPERIMENTAL) Constructs a `field_view` holding a datetime (`this->kind() ==
    /// field_kind::datetime`). \details Results in a `field_view` with value semantics (always
    /// valid).
    BOOST_CXX14_CONSTEXPR explicit inline field_view(const datetime& v) noexcept;

    /// \brief (EXPERIMENTAL) Constructs a `field_view` holding a time (`this->kind() ==
    /// field_kind::time`). \details Results in a `field_view` with value semantics (always valid).
    BOOST_CXX14_CONSTEXPR explicit inline field_view(const time& v) noexcept;

    /// Returns the type of the value this `field_view` is pointing to.
    BOOST_CXX14_CONSTEXPR inline field_kind kind() const noexcept;

    /// Returns whether this `field_view` points to a `NULL` value.
    BOOST_CXX14_CONSTEXPR bool is_null() const noexcept { return kind() == field_kind::null; }

    /// Returns whether this `field_view` points to a int64 value.
    BOOST_CXX14_CONSTEXPR bool is_int64() const noexcept { return kind() == field_kind::int64; }

    /// Returns whether this `field_view` points to a uint64 value.
    BOOST_CXX14_CONSTEXPR bool is_uint64() const noexcept { return kind() == field_kind::uint64; }

    /// Returns whether this `field_view` points to a string value.
    BOOST_CXX14_CONSTEXPR bool is_string() const noexcept { return kind() == field_kind::string; }

    /// Returns whether this `field_view` points to a binary blob.
    BOOST_CXX14_CONSTEXPR bool is_blob() const noexcept { return kind() == field_kind::blob; }

    /// Returns whether this `field_view` points to a float value.
    BOOST_CXX14_CONSTEXPR bool is_float() const noexcept { return kind() == field_kind::float_; }

    /// Returns whether this `field_view` points to a double value.
    BOOST_CXX14_CONSTEXPR bool is_double() const noexcept { return kind() == field_kind::double_; }

    /// Returns whether this `field_view` points to a date value.
    BOOST_CXX14_CONSTEXPR bool is_date() const noexcept { return kind() == field_kind::date; }

    /// Returns whether this `field_view` points to a datetime value.
    BOOST_CXX14_CONSTEXPR bool is_datetime() const noexcept { return kind() == field_kind::datetime; }

    /// Returns whether this `field_view` points to a time value.
    BOOST_CXX14_CONSTEXPR bool is_time() const noexcept { return kind() == field_kind::time; }

    /// \brief Retrieves the underlying value as an `int64` or throws an exception.
    /// \details If `!this->is_int64()`, throws \ref bad_field_access.
    BOOST_CXX14_CONSTEXPR inline std::int64_t as_int64() const;

    /// \brief Retrieves the underlying value as an `uint64` or throws an exception.
    /// \details If `!this->is_uint64()`, throws \ref bad_field_access.
    BOOST_CXX14_CONSTEXPR inline std::uint64_t as_uint64() const;

    /// \brief Retrieves the underlying value as a string or throws an exception.
    /// \details If `!this->is_string()`, throws \ref bad_field_access.
    BOOST_CXX14_CONSTEXPR inline string_view as_string() const;

    /// \brief Retrieves the underlying value as a blob or throws an exception.
    /// \details If `!this->is_blob()`, throws \ref bad_field_access.
    BOOST_CXX14_CONSTEXPR inline blob_view as_blob() const;

    /// \brief Retrieves the underlying value as a `float` or throws an exception.
    /// \details If `!this->is_float()`, throws \ref bad_field_access.
    BOOST_CXX14_CONSTEXPR inline float as_float() const;

    /// \brief Retrieves the underlying value as a `double` or throws an exception.
    /// \details If `!this->is_double()`, throws \ref bad_field_access.
    BOOST_CXX14_CONSTEXPR inline double as_double() const;

    /// \brief Retrieves the underlying value as a `date` or throws an exception.
    /// \details If `!this->is_date()`, throws \ref bad_field_access.
    BOOST_CXX14_CONSTEXPR inline date as_date() const;

    /// \brief Retrieves the underlying value as a `datetime` or throws an exception.
    /// \details If `!this->is_datetime()`, throws \ref bad_field_access.
    BOOST_CXX14_CONSTEXPR inline datetime as_datetime() const;

    /// \brief Retrieves the underlying value as a `time` or throws an exception.
    /// \details If `!this->is_time()`, throws \ref bad_field_access.
    BOOST_CXX14_CONSTEXPR inline time as_time() const;

    /// \brief Retrieves the underlying value as an `int64` (unchecked access).
    /// \details If `!this->is_int64()`, results in undefined behavior.
    BOOST_CXX14_CONSTEXPR inline std::int64_t get_int64() const noexcept;

    /// \brief Retrieves the underlying value as an `uint64` (unchecked access).
    /// \details If `!this->is_uint64()`, results in undefined behavior.
    BOOST_CXX14_CONSTEXPR inline std::uint64_t get_uint64() const noexcept;

    /// \brief Retrieves the underlying value as a string (unchecked access).
    /// \details If `!this->is_string()`, results in undefined behavior.
    BOOST_CXX14_CONSTEXPR inline string_view get_string() const noexcept;

    /// \brief Retrieves the underlying value as a blob (unchecked access).
    /// \details If `!this->is_blob()`, results in undefined behavior.
    BOOST_CXX14_CONSTEXPR inline blob_view get_blob() const noexcept;

    /// \brief Retrieves the underlying value as a `float` (unchecked access).
    /// \details If `!this->is_float()`, results in undefined behavior.
    BOOST_CXX14_CONSTEXPR inline float get_float() const noexcept;

    /// \brief Retrieves the underlying value as a `double` (unchecked access).
    /// \details If `!this->is_double()`, results in undefined behavior.
    BOOST_CXX14_CONSTEXPR inline double get_double() const noexcept;

    /// \brief Retrieves the underlying value as a `date` (unchecked access).
    /// \details If `!this->is_date()`, results in undefined behavior.
    BOOST_CXX14_CONSTEXPR inline date get_date() const noexcept;

    /// \brief Retrieves the underlying value as a `datetime` (unchecked access).
    /// \details If `!this->is_datetime()`, results in undefined behavior.
    BOOST_CXX14_CONSTEXPR inline datetime get_datetime() const noexcept;

    /// \brief Retrieves the underlying value as a `time` (unchecked access).
    /// \details If `!this->is_time()`, results in undefined behavior.
    BOOST_CXX14_CONSTEXPR inline time get_time() const noexcept;

    /// \brief Tests for equality.
    /// \details If one of the operands is a `uint64` and the other a
    /// `int64`, and the values are equal, returns `true`. Otherwise, if the types are
    /// different, returns always `false` (`float` and `double` values are considered to be
    /// different between them). `NULL` values are equal to other `NULL` values.
    BOOST_CXX14_CONSTEXPR inline bool operator==(const field_view& rhs) const noexcept;

    /// Tests for inequality.
    BOOST_CXX14_CONSTEXPR bool operator!=(const field_view& rhs) const noexcept { return !(*this == rhs); }

private:
    BOOST_CXX14_CONSTEXPR explicit field_view(detail::string_view_offset v, bool is_blob) noexcept
        : ikind_(is_blob ? internal_kind::sv_offset_blob : internal_kind::sv_offset_string), repr_(v)
    {
    }

    BOOST_CXX14_CONSTEXPR explicit field_view(const detail::field_impl* v) noexcept
        : ikind_(internal_kind::field_ptr), repr_(v)
    {
    }

    enum class internal_kind
    {
        null = 0,
        int64,
        uint64,
        string,
        blob,
        float_,
        double_,
        date,
        datetime,
        time,
        sv_offset_string,
        sv_offset_blob,
        field_ptr
    };

    union repr_t
    {
        std::int64_t int64;
        std::uint64_t uint64;
        string_view string;
        blob_view blob;
        float float_;
        double double_;
        date date_;
        datetime datetime_;
        time time_;
        detail::string_view_offset sv_offset_;
        const detail::field_impl* field_ptr;

        BOOST_CXX14_CONSTEXPR repr_t() noexcept : int64{} {}
        BOOST_CXX14_CONSTEXPR repr_t(std::int64_t v) noexcept : int64(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(std::uint64_t v) noexcept : uint64(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(string_view v) noexcept : string{v} {}
        BOOST_CXX14_CONSTEXPR repr_t(blob_view v) noexcept : blob{v} {}
        BOOST_CXX14_CONSTEXPR repr_t(float v) noexcept : float_(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(double v) noexcept : double_(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(date v) noexcept : date_(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(datetime v) noexcept : datetime_(v) {}
        BOOST_CXX14_CONSTEXPR repr_t(time v) noexcept : time_(v.count()) {}
        BOOST_CXX14_CONSTEXPR repr_t(detail::string_view_offset v) noexcept : sv_offset_{v} {}
        BOOST_CXX14_CONSTEXPR repr_t(const detail::field_impl* v) noexcept : field_ptr(v) {}
    };

    internal_kind ikind_{internal_kind::null};
    repr_t repr_;

    BOOST_CXX14_CONSTEXPR bool is_field_ptr() const noexcept { return ikind_ == internal_kind::field_ptr; }
    BOOST_CXX14_CONSTEXPR inline void check_kind(internal_kind expected) const;

#ifndef BOOST_MYSQL_DOXYGEN
    friend class field;
    friend struct detail::field_view_access;
    friend std::ostream& operator<<(std::ostream& os, const field_view& v);
#endif
};

/**
 * \relates field_view
 * \brief Streams a `field_view`.
 */
inline std::ostream& operator<<(std::ostream& os, const field_view& v);

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/field_view.hpp>

#endif
