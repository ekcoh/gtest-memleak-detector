// Copyright(C) 2019 - 2020 H�kan Sidenvall <ekcoh.git@gmail.com>.
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

#include <memory>               // std::unique_ptr

#pragma warning(push)
#pragma warning(disable: 26812) // MSVC C26812: unscoped enum
#pragma warning(disable: 26495) // MSVC C26495: unitialized variable
#include <gtest/gtest.h>        // Google Test
#pragma warning(pop)

// Memory debugging tools (MSVC only)
#if defined(_DEBUG) && defined(_MSC_VER) && defined(_WIN32)
#define GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
#define GTEST_MEMLEAK_DETECTOR_CRTDBG_AVAILABLE
#endif

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

namespace gtest_memleak_detector { 

///////////////////////////////////////////////////////////////////////////////
// MemoryLeakDetectorListener
///////////////////////////////////////////////////////////////////////////////

class MemoryLeakDetector;

class MemoryLeakDetectorListener : public ::testing::EmptyTestEventListener {
public:
	MemoryLeakDetectorListener(int argc = 0, char** argv = nullptr);
	virtual ~MemoryLeakDetectorListener() noexcept;

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

private:
	std::unique_ptr<MemoryLeakDetector> impl_;
};

} // namespace gtest_memleak_detector

#endif // GTEST_MEMLEAK_DETECTOR_H
