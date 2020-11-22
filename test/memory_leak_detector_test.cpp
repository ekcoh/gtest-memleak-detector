// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file
// found in the root directory of this distribution.

#include "memory_leak_detector_test.h"

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

memory_leak_detector_test::memory_leak_detector_test()
    : detector(1, argv)
{ }

void memory_leak_detector_test::TearDown()
{
    std::remove(MemoryLeakDetectorListener::MakeDatabaseFilePath(test_binary_path).c_str());
}

void memory_leak_detector_test::GivenPreTestSequence()
{   // This is equivalent to the sequence executed by google test when
    // executing test
    detector.OnTestProgramStart(instance());
    detector.OnTestSuiteStart(*instance().current_test_suite());
    detector.OnTestStart(*instance().current_test_info());
}

void memory_leak_detector_test::GivenPostTestSequence(expected_outcome action, 
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

void memory_leak_detector_test::EndTest()
{
    detector.OnTestEnd(*instance().current_test_info());
}

testing::UnitTest& memory_leak_detector_test::instance()
{
    return *testing::UnitTest::GetInstance();
}
