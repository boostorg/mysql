//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <chrono>

#include "test_unit/mock_timer.hpp"

using std::chrono::steady_clock;

// The current time
static thread_local steady_clock::time_point g_mock_now{};

boost::mysql::test::mock_clock::time_point boost::mysql::test::mock_clock::now() { return g_mock_now; }

void boost::mysql::test::mock_clock::advance_time_by(steady_clock::duration dur) { g_mock_now += dur; }
