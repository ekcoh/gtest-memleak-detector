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

#ifndef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H

#include <gtest_memleak_detector/gtest_memleak_detector.h>

// Memory debugging tools (MSVC only)
#if defined(_DEBUG) && defined(_MSC_VER)
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE
	
// CRT debug tools
#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif // _CRTDBG_MAP_ALLOC

#include <crtdbg.h>

#include <vector>
#include <string>
#include <fstream>

namespace gtest_memleak_detector
{
	struct allocation_trace
	{
		std::string id;
		long alloc_no;
	};

	class MemoryLeakDetectorListener::Impl
	{
	public:
		Impl(int argc, char** argv0);
		~Impl();

		void OnTestProgramStart(const ::testing::UnitTest& unit_test);
		void OnTestStart(const ::testing::TestInfo& test_info);
		void OnTestEnd(const ::testing::TestInfo& test_info);
		void OnTestProgramEnd(const ::testing::UnitTest& unit_test);

		static const long no_break_alloc;

	private:
		static int output_redirect(int, char*, int*);
		static long parsed_alloc_no;

	private:
		_CrtMemState pre_state_;
		_CRT_REPORT_HOOK output_redirect_hook_;
		long break_alloc_;
		std::ifstream in_;
		std::ofstream out_;
		struct _stat file_info_;
	};
} // namespace gtest_policy

#else

#ifdef _MSC_VER
#pragma message ( \
	"WARNING: Memory leak detection not supported by this compiler/configuration/" \
	"platform combination. All memory leak assertions will be disabled. " \
	"This may be caused by a non-debug build, e.g. release build.")
#endif // _MSC_VER

namespace gtest_policy
{
	class MemoryLeakDetectorListener::Impl { };
} // namespace gtest_policy

#endif // defined(_DEBUG) && defined(_MSC_VER)

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H