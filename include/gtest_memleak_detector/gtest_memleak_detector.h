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

namespace gtest_memleak_detector {

///////////////////////////////////////////////////////////////////////////////
// MemoryLeakDetectorListener
///////////////////////////////////////////////////////////////////////////////

class MemoryLeakDetectorListener : public ::testing::EmptyTestEventListener {
public:
	MemoryLeakDetectorListener(int argc = 0, char** argv = nullptr);
	virtual ~MemoryLeakDetectorListener();

	virtual void OnTestProgramStart(
		const ::testing::UnitTest& unit_test) override;
	virtual void OnTestStart(
		const ::testing::TestInfo& test_info) override;
	virtual void OnTestEnd(
		const ::testing::TestInfo& test_info) override;
	virtual void OnTestProgramEnd(
		const ::testing::UnitTest& unit_test) override;
private:
	class Impl;
	std::unique_ptr<Impl> impl_; 
};

} // namespace gtest_memleak_detector

#endif // GTEST_MEMLEAK_DETECTOR_H