// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

//
// Policy Extensions for the Google C++ Testing and Mocking Framework (Google Test).
//
// This header file defines the public API for Google Test Policy Extensions. 
// It should be included by any test program that uses Google Test Policy Extension.
//
// IMPORTANT NOTE: Due to limitation of the C++ language, we have to
// leave some internal implementation details in this header file.
// They are clearly marked by comments like this:
//
//   // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
//
// Such code is NOT meant to be used by a user directly, and is subject
// to CHANGE WITHOUT NOTICE. Therefore DO NOT DEPEND ON IT in a user program!
//
// Code style used is same as Google Test source code to make source code blend.

#ifndef GTEST_MEMLEAK_DETECTOR_H
#define GTEST_MEMLEAK_DETECTOR_H

#include <gtest/gtest.h> // Google Test
#include <memory>        // std::unique_ptr

namespace gtest_memleak_detector {

// TODO Implement mocking of memory allocations to verify that failed allocation
// is handled properly, e.g.
// GIVEN_FAILED_ALLOCATION(index) based on parameterized test, can be done as e.g. system test

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