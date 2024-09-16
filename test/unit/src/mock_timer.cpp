//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <chrono>

#include "test_unit/mock_timer.hpp"

namespace {

thread_local std::chrono::steady_clock::time_point g_mock_now{};

}  // namespace

boost::mysql::test::mock_clock::time_point boost::mysql::test::mock_clock::now() { return g_mock_now; }

std::chrono::steady_clock::time_point boost::mysql::test::mock_timer_service::current_time() const
{
    return g_mock_now;
}

void boost::mysql::test::mock_timer_service::set_current_time(std::chrono::steady_clock::time_point to)
{
    g_mock_now = to;
}