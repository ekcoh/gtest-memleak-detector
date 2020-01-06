// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

//
// Policy Extensions for the Google C++ Testing and Mocking Framework (Google Test).
//
// This header file defines the public API for Google Test Policy Extensions. 
// It should be included by any test program that uses Google Test Policy Extension.
//
// Code style used is same as Google Test source code to make source code blend.

#include "memory_leak_detector_listener_impl.h"

#include <fstream>
#include <Windows.h>

#ifndef GTEST_MEMLEAK_DETECTOR_FILENAME
#define GTEST_MEMLEAK_DETECTOR_FILENAME "gtest_memleak_detector.allocation"
#endif // GTEST_MEMLEAK_DETECTOR_FILENAME

#ifndef GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME
#define GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME GTEST_MEMLEAK_DETECTOR_FILENAME ## ".tmp"
#endif // GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE

const long gtest_memleak_detector::MemoryLeakDetectorListener::Impl::no_break_alloc = -1;

long gtest_memleak_detector::MemoryLeakDetectorListener::Impl::parsed_alloc_no = -1;

int gtest_memleak_detector::MemoryLeakDetectorListener::Impl::Impl::output_redirect(
	int, char* szMsg, int*)
{ 
	const auto start = strchr(szMsg, '{');
	if (start != nullptr)
	{
		const auto stop = strchr(start, '}');
		if (stop != nullptr)
		{
			const auto len = stop - start - 1;
			if (len > 0)
			{
				char buffer[32];
				strncpy_s(buffer, start + 1, len);
				auto allocation_no = strtol(buffer, nullptr, 10);
				if (allocation_no == LONG_MIN || allocation_no == LONG_MAX)
					allocation_no = 0L;
				parsed_alloc_no = allocation_no;
			}
		}
	}

	std::cerr << szMsg; 
	return 1; // TRUE
}

gtest_memleak_detector::MemoryLeakDetectorListener::Impl::Impl(int argc, char** argv) :
	pre_state_{ 0 },
	output_redirect_hook_(output_redirect),
	break_alloc_(no_break_alloc)
{
	if (argc > 0)
	{
		if (_stat(argv[0], &file_info_) != 0)
			memset(&file_info_, 0, sizeof(struct _stat));
		//std::ifstream bin(path);
		//bin.close();
	}

	out_.open(GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME);

	out_ << file_info_.st_size << '\n' << file_info_.st_mtime << '\n';
	for (auto i = 0; i < argc; ++i)
		out_ << argv[0] << '\n';
	//out_ << test.test_suite_to_run_count() << '\n';
	//out_ << test.test_case_to_run_count() << '\n';

	// Show warning message in case break_alloc is set to prevent 
	// accidental commits with break alloc set.
	if (IsDebuggerPresent() && break_alloc_ != no_break_alloc)
	{
		std::cerr << "WARNING: Memory leak detection allocation break is set."
			"This should only be set while debugging memory leaks." << std::endl;
	}

	// Turn on debug allocation 
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
}

gtest_memleak_detector::MemoryLeakDetectorListener::Impl::~Impl()
{
	// Empty
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestStart(
	const ::testing::TestInfo& /*test_info*/)
{
	_CrtMemCheckpoint(&pre_state_);

	// Set to number reported in braces in leak failure report, e.g.
	// ... {2522} normal block at 0x036B3C18, 4 bytes long. ...
	// #warning Memory debugging break-point active
	if (IsDebuggerPresent() && break_alloc_ != no_break_alloc)
	{
		_CrtSetBreakAlloc(break_alloc_);
		break_alloc_ = no_break_alloc;
	}
}

//int AllocationHook(int allocType, void *userData, size_t size,
//	int blockType, long requestNumber,
//	const unsigned char *filename, int lineNumber)
//{
//	//if (allocType == _HOOK_FREE)
//	//{
//	//	
//	//}
//	return 1; // TRUE
//}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestEnd(
	const ::testing::TestInfo& test_info)
{
	// Avoid adding extra asserts if test is not passing anyway and the failing logic is the main failure.
	// Another reason to do this is that gtest report false positives when reporting assertion failures
	// due to memory allocation within gtest framework (bad).
	if (!test_info.result()->Passed())
		return;

	// Create memory checkpoint
	_CrtMemState post_state;
	_CrtMemCheckpoint(&post_state);

	//const auto test_allocation_count = post_state_.lTotalCount - pre_state_.lTotalCount;
	//memory_was_allocated_ = test_allocation_count != 0;

	// Compute a diff from pre/post test callbacks and dump statistics for that scope only if leak is detected
	_CrtMemState mem_diff;
	const auto leak_detected = _CrtMemDifference(&mem_diff, &pre_state_, &post_state);
	if (!leak_detected)
		return;

	// Dump objects and statistics via redirect hook
	std::cerr << "Failure: Memory leak detected.\n\n";
	_CrtMemDumpAllObjectsSince(&pre_state_);
	_CrtMemDumpStatistics(&mem_diff);

	// Parse allocation id from dump
	if (parsed_alloc_no != 0)
	{
		out_ << parsed_alloc_no;
		out_ << ",";
		out_ << test_info.test_suite_name();
		out_ << "::";
		out_ << test_info.name();
		if (test_info.value_param() != nullptr)
		{
			out_ << '/';
			out_ << test_info.value_param();
		}
		if (test_info.type_param() != nullptr)
		{
			out_ << '/';
			out_ << test_info.type_param();
		}
		out_ << '\n';
	}

	// Assertion that will fail any test leaking memory
	GTEST_NONFATAL_FAILURE_("Memory leak detected");
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestProgramStart(
	const ::testing::UnitTest& test)
{
	const auto test_count = test.total_test_count();
	//database_.reserve(test_count);


	// TODO Write configuration and binary information, debugger attached or not seems to result in different allocations?!

	if (IsDebuggerPresent())
	{
		in_.open(GTEST_MEMLEAK_DETECTOR_FILENAME);
		in_ >> break_alloc_;
	}

	_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, output_redirect_hook_);
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestProgramEnd(
	const ::testing::UnitTest& /*test*/)
{
	_CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, output_redirect_hook_);

	out_.close();
	in_.close();

	// Remove previous file
	std::remove(GTEST_MEMLEAK_DETECTOR_FILENAME);

	// Rename temp output file to be input file for next run
	std::rename(GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME, GTEST_MEMLEAK_DETECTOR_FILENAME);
}

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE