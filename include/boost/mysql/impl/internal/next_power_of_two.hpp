//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_NEXT_POWER_OF_TWO_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_NEXT_POWER_OF_TWO_HPP

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

template<int Shift, class UnsignedInt, bool Continue>
struct recursive_shift_or;

// Apply shift and continue to the next power of 2
template<int Shift, class UnsignedInt>
struct recursive_shift_or<Shift, UnsignedInt, true>
{
    static void apply(UnsignedInt& n)
    {
        n |= n >> Shift;
        recursive_shift_or<Shift * 2, UnsignedInt, (Shift * 2 < sizeof(UnsignedInt) * 8)>::apply(n);
    }
};

// Stop recursion when shift exceeds type width
template<int Shift, class UnsignedInt>
struct recursive_shift_or<Shift, UnsignedInt, false>
{
    static void apply(UnsignedInt&) {}
};

// Returns the smallest power of two greater than or equal to n
template<class UnsignedInt>
UnsignedInt next_power_of_two(UnsignedInt n) noexcept
{
    static_assert(std::is_unsigned<UnsignedInt>::value, "");
    if (n == 0) return 1;
    n--;
    // Fill all lower bits
    recursive_shift_or<1, UnsignedInt, (1 < sizeof(UnsignedInt) * 8)>::apply(n);
    return n + 1;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif