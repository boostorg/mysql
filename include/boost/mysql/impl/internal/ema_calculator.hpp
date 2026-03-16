//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_EMA_CALCULATOR_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_EMA_CALCULATOR_HPP

#include <boost/assert.hpp>
#include <boost/config.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

/**
 * \brief Integer-based Exponential Moving Average (EMA).
 * 
 * This class implements a fixed-point EMA using bitwise shifts.
 * It maintains internal precision by scaling the value.
 * 
 * Formula: average_new = average_old - (average_old >> shift) + next_value
 * 
 * \tparam Shift The smoothing factor exponent (alpha = 1 / 2^shift).
 *               Higher values result in more smoothing.
 */
template <std::size_t Shift>
class ema_calculator
{   
    // std::uint64_t provides a strong guarantee against overflow
    using IntType = std::uint64_t;

    static_assert(Shift > 0, "Shift must be greater than 0.");
    static_assert(Shift < (sizeof(IntType) * 4), "Shift exceeds half the bit-width of the type.");

    static BOOST_INLINE_CONSTEXPR IntType max_value = std::numeric_limits<IntType>::max() >> Shift;

    IntType average_;

public:
    ema_calculator(IntType initial_value) : average_(initial_value)
    {
        BOOST_ASSERT(initial_value <= max_value);
    }

    void update(IntType next_value) noexcept 
    {
        BOOST_ASSERT(next_value <= max_value);
        average_ = average_ - (average_ >> Shift) + next_value;
    }

    inline IntType get_average() const noexcept 
    {
        // Return the average by scaling back down
        return average_ >> Shift; 
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
