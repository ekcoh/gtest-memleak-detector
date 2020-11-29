// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file
// found in the root directory of this distribution.

#include <fstream>
#include "memory_leak_detector_listener_test.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#pragma warning( push )
// warning C5039: potentially throwing function passed to extern C function 
// under -EHc. May result in undefined behavior.
#pragma warning( disable : 5039 ) 
#include <Windows.h>
#pragma warning( pop )

const char* test_binary_path = "test.exe";
char* argv[] = { "test.exe" };

memory_leak_detector_listener_test::memory_leak_detector_listener_test()
    : detector(1, argv)
{ }

void memory_leak_detector_listener_test::SetUp()
{   
    // Ensure database do not exist
    std::ifstream f;
    f.open(MemoryLeakDetectorListener::MakeDatabaseFilePath(test_binary_path).c_str());
    ASSERT_FALSE(f) << "Test assumption failed: No pre-existing database exists";
}

void memory_leak_detector_listener_test::TearDown()
{
    // Remove database in-between each test
    std::remove(MemoryLeakDetectorListener::MakeDatabaseFilePath(test_binary_path).c_str());
}

void memory_leak_detector_listener_test::GivenPreTestSequence()
{   // This is equivalent to the sequence executed by google test when
    // executing test
    detector.OnTestProgramStart(instance());
    detector.OnTestSuiteStart(*instance().current_test_suite());
    detector.OnTestStart(*instance().current_test_info());
}

void memory_leak_detector_listener_test::GivenPostTestSequence(expected_outcome action, 
    const char* failure_message)
{   // This is equivalent to the sequence executed by google test when
    // executing tests but asserts failure based on action.
    if (action == expected_outcome::mem_leak_failure)
        EXPECT_NONFATAL_FAILURE(EndTest(), failure_message);
    else
        EndTest();
    detector.OnTestSuiteEnd(*instance().current_test_suite());
    detector.OnTestProgramEnd(instance());
}

void memory_leak_detector_listener_test::EndTest()
{
    detector.OnTestEnd(*instance().current_test_info());
}

testing::UnitTest& memory_leak_detector_listener_test::instance()
{
    return *testing::UnitTest::GetInstance();
}
