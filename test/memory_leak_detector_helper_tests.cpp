// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#include <gtest_memleak_detector/gtest_memleak_detector.h>

#include "memory_leak_detector_test.h" // fixture

#define GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART \
    "Memory leak detected."
#define GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART \
    "\n- Allocation request no: "
#define GTEST_MEMLEAK_DETECTOR_ORIGIN_MSG_PART \
    "\n- Origin: "
#define GTEST_MEMLEAK_DETECTOR_STACKTRACE_MSG_PART \
    "\n\nStacktrace:\n"
#define GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART \
    "\n\n(Re-run the test again to obtain stack-trace for the allocation " \
    "causing the memory-leak.)"

TEST_F(memory_leak_detector_test,
    make_database_file_path__should_return_same_path_with_additional_suffix__if_given_valid_string)
{
    EXPECT_STREQ(MemoryLeakDetectorListener::MakeDatabaseFilePath("test.exe").c_str(),
        "test.exe.gt.memleaks");
    EXPECT_STREQ(MemoryLeakDetectorListener::MakeDatabaseFilePath("c:\\something\\test.exe").c_str(),
        "c:\\something\\test.exe.gt.memleaks");
    EXPECT_STREQ(MemoryLeakDetectorListener::MakeDatabaseFilePath("/user/myuser/test").c_str(),
        "/user/myuser/test.gt.memleaks");
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_all_info__if_given_only_valid_input)
{
    EXPECT_STREQ(MemoryLeakDetectorListener::MakeFailureMessage(
        1234, "somefile.cpp", 67, "stacktrace_data").c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART "1234"
        GTEST_MEMLEAK_DETECTOR_ORIGIN_MSG_PART "somefile.cpp: 67"
        GTEST_MEMLEAK_DETECTOR_STACKTRACE_MSG_PART "stacktrace_data");
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_only_info_about_leak__if_given_all_valid_input_except_stacktrace)
{
    EXPECT_STREQ(MemoryLeakDetectorListener::MakeFailureMessage(
        3456, "somefile.cpp", 51, nullptr).c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART "3456"
        GTEST_MEMLEAK_DETECTOR_ORIGIN_MSG_PART "somefile.cpp: 51"
        GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART);
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_only_info_and_stacktrace__if_given_all_valid_input_except_filename_and_line)
{
    EXPECT_STREQ(MemoryLeakDetectorListener::MakeFailureMessage(
        3456, nullptr, 0, "stacktrace_data").c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART "3456"
        GTEST_MEMLEAK_DETECTOR_STACKTRACE_MSG_PART "stacktrace_data");
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_only_allocation_request__if_all_strings_are_nullptr_but_valid_alloc_no)
{
    EXPECT_STREQ(MemoryLeakDetectorListener::MakeFailureMessage(3456, nullptr, 0, nullptr).c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART "3456"
        GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART);
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_only_leak_info__if_all_strings_are_nullptr_and_invalid_alloc_no)
{
    EXPECT_STREQ(MemoryLeakDetectorListener::MakeFailureMessage(
        -1, nullptr, 0, nullptr).c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART 
        GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART);
    EXPECT_STREQ(MemoryLeakDetectorListener::MakeFailureMessage(
        -2, nullptr, 0, nullptr).c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART 
        GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART);
}



