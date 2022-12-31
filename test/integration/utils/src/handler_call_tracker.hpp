//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_SRC_HANDLER_CALL_TRACKER_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_SRC_HANDLER_CALL_TRACKER_HPP

#include <thread>
#include <boost/test/unit_test.hpp>

namespace boost {
namespace mysql {
namespace test {

class handler_call_tracker
{
    int call_count_ {};
    std::thread::id calling_thread_ {};
public:
    handler_call_tracker() = default;
    void register_call()
    {
        ++call_count_;
        calling_thread_ = std::this_thread::get_id();
    }
    int call_count() const { return call_count_; }
    std::thread::id calling_thread() const { return calling_thread_; }
    void verify()
    {
        BOOST_TEST(call_count() == 1); // we call handler exactly once
        BOOST_TEST(calling_thread() != std::this_thread::get_id()); // handler runs in the io_context thread
    }
};


}
}
}

#endif