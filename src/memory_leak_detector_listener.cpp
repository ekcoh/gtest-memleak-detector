// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution..

#include <gtest_memleak_detector/gtest_memleak_detector.h>
#include "memory_leak_detector_listener_impl.h"

const char* gtest_memleak_detector::MemoryLeakDetectorListener::database_file_suffix
    = "gt.memleaks";

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
gtest_memleak_detector::MemoryLeakDetectorListener::MemoryLeakDetectorListener(
    int argc, char** argv)
    : impl_(nullptr)
{
	impl_ = std::make_unique<Impl>(argc, argv);
}
#else
gtest_memleak_detector::MemoryLeakDetectorListener::MemoryLeakDetectorListener(
    int, char**)
    : impl_(nullptr)
{ }
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

gtest_memleak_detector::MemoryLeakDetectorListener::~MemoryLeakDetectorListener() noexcept
{
	// Empty but required for PIMPL idiom since opaque type in header
}

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestProgramStart(
	const ::testing::UnitTest& unit_test)
{
    (void)unit_test;
}
#else
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestProgramStart(
	const ::testing::UnitTest&) {}
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestStart(
	const ::testing::TestInfo& test_info)
{
    // IMPORTANT: A factory-like functor is used to postpone string allocation 
    //            until memory checkpoint has been established.
    //            If string would be allocated here it would be reported as a
    //            memory leak which would be a false positive.
	impl_->Start([&]() { return DescribeTest(test_info); });
}
#else
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestStart(
	const ::testing::TestInfo&) {}
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestEnd(
	const ::testing::TestInfo& test_info)
{
	// IMPORTANT: A factory-like functor is used to postpone string allocation 
    //            until memory checkpoint has been established.
    //            If string would be allocated here it would be reported as a
    //            memory leak which would be a false positive.
    impl_->End([&]() { return DescribeTest(test_info); },
        test_info.result()->Passed());
}
#else
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestEnd(
	const ::testing::TestInfo&) {}
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestProgramEnd(
	const ::testing::UnitTest& unit_test)
{
    (void)unit_test;
    impl_->WriteDatabase();
}
#else
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestProgramEnd(
	const ::testing::UnitTest&) {}
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

std::string
gtest_memleak_detector::MemoryLeakDetectorListener::DescribeTest(
    const ::testing::TestInfo& test_info)
{
    std::stringstream ss;
    ss << test_info.test_suite_name();
    ss << "::";
    ss << test_info.name();
    if (test_info.value_param() != nullptr)
    {
        ss << '/';
        ss << test_info.value_param();
    }
    if (test_info.type_param() != nullptr)
    {
        ss << '/';
        ss << test_info.type_param();
    }
    return ss.str();
}

std::string gtest_memleak_detector::MemoryLeakDetectorListener::MakeDatabaseFilePath(
	const char* binary_file_path)
{
	if (!binary_file_path)
		throw std::exception();
	std::string path = binary_file_path;
	path += '.';
	path += database_file_suffix;
	return path;
}

std::string gtest_memleak_detector::MemoryLeakDetectorListener::MakeFailureMessage(
    long leak_alloc_no, 
    const char* leak_file, 
    unsigned long leak_line, 
    const char* leak_trace)
{
    std::stringstream ss;
    ss << "Memory leak detected.";
    if (leak_alloc_no >= 0)
        ss << "\n- Allocation request no: " << leak_alloc_no;
    if (leak_file != nullptr && leak_file[0] != 0)
    {
        ss << "\n- Origin: " << leak_file;
        if (leak_line != static_cast<unsigned long>(-1))
            ss << ": " << leak_line;
    }
    if (leak_trace && leak_trace[0] != 0)
    {
        ss << "\n\nStacktrace:\n" << leak_trace;
    }
    else
    {
        ss << "\n\n(Re-run the test again to obtain stack-trace for the "
            "allocation causing the memory-leak.)";
    }
    return ss.str();
}

void gtest_memleak_detector::MemoryLeakDetectorListener::FailCurrentTest(
    long leak_alloc_no, 
    const char* leak_file, 
    unsigned long leak_line, 
    const char* leak_trace)
{
    const auto message = MakeFailureMessage(leak_alloc_no, leak_file, leak_line, leak_trace);
    GTEST_MESSAGE_AT_(leak_file, static_cast<int>(leak_line), 
        message.c_str(), ::testing::TestPartResult::kNonFatalFailure);
}
