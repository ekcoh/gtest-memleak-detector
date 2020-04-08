// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file
// found in the root directory of this distribution.

#include "memory_leak_detector_test.h"

void memory_leak_detector_test::SetUp()
{
    p_ = nullptr;
}

void memory_leak_detector_test::TearDown()
{
    // empty
}

void memory_leak_detector_test::GivenPreTestSequence()
{
    detector.OnTestProgramStart(instance());
    detector.OnTestSuiteStart(*instance().current_test_suite());
    detector.OnTestStart(*instance().current_test_info());
}

void memory_leak_detector_test::GivenPostTestSequence(expected_outcome action)
{
    if (action == expected_outcome::mem_leak_failure)
        EXPECT_NONFATAL_FAILURE(EndTest(), "Memory leak detected");
    else
        EndTest();
    detector.OnTestSuiteEnd(*instance().current_test_suite());
    detector.OnTestProgramEnd(instance());
}

void memory_leak_detector_test::GivenMemoryAllocated(allocation_type method)
{
    switch (method)
    {
    case allocation_type::malloc_free:
        p_ = static_cast<int*>(malloc(sizeof(int)));
        break;
    case allocation_type::new_delete:
    default:
        p_ = new int();
        break;
    }
}

void memory_leak_detector_test::GivenMemoryFreed(allocation_type method)
{
    if (!p_) return;

    switch (method)
    {
    case allocation_type::malloc_free:
        free(p_);
        break;
    case allocation_type::new_delete:
    default:
        delete p_;
        break;
    }

    p_ = nullptr;
}

void memory_leak_detector_test::EndTest()
{
    detector.OnTestEnd(*instance().current_test_info());
}

testing::UnitTest& memory_leak_detector_test::instance()
{
    return *testing::UnitTest::GetInstance();
}