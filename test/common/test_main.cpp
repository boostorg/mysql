//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include <memory>

using namespace testing;

namespace
{

class reduced_listener : public testing::TestEventListener
{
    std::unique_ptr<TestEventListener> default_listener_;
public:
    explicit reduced_listener(std::unique_ptr<TestEventListener> default_listener) :
        default_listener_(std::move(default_listener)) {}


    void OnTestProgramStart(const UnitTest& unit_test) override
    {
        default_listener_->OnTestProgramStart(unit_test);
    }

    void OnTestIterationStart(const UnitTest& unit_test, int iteration) override
    {
        default_listener_->OnTestIterationStart(unit_test, iteration);
    }

    void OnEnvironmentsSetUpStart(const UnitTest&) override {}

    void OnEnvironmentsSetUpEnd(const UnitTest&) override {}

    void OnTestCaseStart(const TestCase&) override {}

    void OnTestStart(const TestInfo&) override {}

    void OnTestPartResult(const TestPartResult& part) override
    {
        // This is the code that prints what happened on a failure
        if (part.failed() || part.skipped())
        {
            default_listener_->OnTestPartResult(part);
        }
    }

    void OnTestEnd(const TestInfo& test_info) override
    {
        if (test_info.result()->Failed())
        {
            default_listener_->OnTestEnd(test_info);
        }
    }

    void OnTestCaseEnd(const TestCase&) override {}

    void OnEnvironmentsTearDownStart(const UnitTest&) override {}

    void OnEnvironmentsTearDownEnd(const UnitTest&) override {}

    void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override
    {
        default_listener_->OnTestIterationEnd(unit_test, iteration);
    }

    void OnTestProgramEnd(const UnitTest& unit_test) override
    {
        default_listener_->OnTestProgramEnd(unit_test);
    }
};

}

int main(int argc, char** argv)
{
    InitGoogleTest(&argc, argv);

    // remove the default listener and insert ours
    TestEventListeners& listeners = UnitTest::GetInstance()->listeners();
    std::unique_ptr<TestEventListener> default_printer (
        listeners.Release(listeners.default_result_printer()));
    listeners.Append(new reduced_listener(std::move(default_printer)));

    // run
    return RUN_ALL_TESTS();
}

