// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file found in 
// the root directory of this distribution.

// Memory leak detector extension for the Google C++ Testing and  Mocking 
// Framework (Google Test).
//
// This header file defines the public API for the Google Test Memory Leak 
// Detector Extension API. It should be included by any test program that 
// uses Google Test Memory Leak Detector Extension.
//
// Code style used is same as Google Test source code to make source code 
// blend.

#ifndef GTEST_MEMLEAK_DETECTOR_H
#define GTEST_MEMLEAK_DETECTOR_H

#include <gtest/gtest.h> // Google Test
#include <memory>        // std::unique_ptr

#define GTEST_MEMLEAK_DETECTOR_APPEND_LISTENER \
  ::testing::UnitTest::GetInstance()->listeners().Append( \
    new gtest_memleak_detector::MemoryLeakDetectorListener(argc, argv)) 

#define GTEST_MEMLEAK_DETECTOR_MAIN \
int main(int argc, char **argv) \
{ \
  ::testing::InitGoogleTest(&argc, argv); \
  ::testing::UnitTest::GetInstance()->listeners().Append( \
    new gtest_memleak_detector::MemoryLeakDetectorListener(argc, argv)); \
  return RUN_ALL_TESTS(); \
}

// Have a max limit on stack trace size to avoid allocating memory as part
// of generating stack trace. Size is in bytes and includes null-termination.
#ifndef GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH 
#define GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH 4096 * 4
#endif // GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH

#ifndef GTEST_MEMLEAK_DETECTOR_PATH_MAX_LENGTH
#define GTEST_MEMLEAK_DETECTOR_PATH_MAX_LENGTH 1024
#endif

namespace gtest_memleak_detector { 

///////////////////////////////////////////////////////////////////////////////
// MemoryLeakDetectorListener
///////////////////////////////////////////////////////////////////////////////

class MemoryLeakDetectorListener : public ::testing::EmptyTestEventListener {
public:
	MemoryLeakDetectorListener(int argc = 0, char** argv = nullptr);
	virtual ~MemoryLeakDetectorListener();

	MemoryLeakDetectorListener(const MemoryLeakDetectorListener&) = delete;
	MemoryLeakDetectorListener(MemoryLeakDetectorListener&&) = delete;

	MemoryLeakDetectorListener& operator=(
		const MemoryLeakDetectorListener&) = delete;
	MemoryLeakDetectorListener& operator=(
		MemoryLeakDetectorListener&&) = delete;

	void OnTestProgramStart(
		const ::testing::UnitTest& unit_test) override;
	void OnTestStart(
		const ::testing::TestInfo& test_info) override;
	void OnTestEnd(
		const ::testing::TestInfo& test_info) override;
	void OnTestProgramEnd(
		const ::testing::UnitTest& unit_test) override;

	static std::string MakeDatabaseFilePath(const char* binary_file_path);
	static std::string MakeFailureMessage(long leak_alloc_no,
		const char* leak_file, 
		unsigned long leak_line, 
		const char* leak_trace);

	static const char* database_file_suffix; // TODO Consider removing!?
private:
    static std::string DescribeTest(const ::testing::TestInfo& test_info);
    
    static void FailCurrentTest(long leak_alloc_no, 
		const char* leak_file, 
		unsigned long leak_line, 
		const char* leak_trace);

	class Impl;
	friend class Impl;
	std::unique_ptr<Impl> impl_; 
};

} // namespace gtest_memleak_detector

#endif // GTEST_MEMLEAK_DETECTOR_H
