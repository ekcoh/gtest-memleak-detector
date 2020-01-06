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

#include <gtest_memleak_detector/gtest_memleak_detector.h>

#include <gtest/gtest-spi.h> // enables testing test failures
#include <memory>

using namespace gtest_memleak_detector;

enum class allocation_type
{
	new_delete,
	malloc_free
};

class memory_leak_detector_test : public ::testing::TestWithParam<allocation_type>
{
public:
	void SetUp()
	{
		expect_memory_leak_ = false;
		p_ = nullptr;

		detector.OnTestProgramStart(instance());
		detector.OnTestSuiteStart(*instance().current_test_suite());
		detector.OnTestStart(*instance().current_test_info());
	}

	void TearDown()
	{
		if (expect_memory_leak_)
			EXPECT_NONFATAL_FAILURE(EndTest(), "Memory leak detected");
		else
			EndTest();
		detector.OnTestSuiteEnd(*instance().current_test_suite());
		detector.OnTestProgramEnd(instance());
		free_memory();
	}

	void expect_memory_leak_failure(bool should_fail) { expect_memory_leak_ = should_fail; }

	void allocate_memory()
	{
		switch (GetParam())
		{
		case allocation_type::malloc_free:
			p_ = static_cast<int*>(malloc(sizeof(int)));
			break;
		case allocation_type::new_delete:
		default:
			p_ = new int();
			break;
		}
	}

	void free_memory()
	{
		if (!p_) return;

		switch (GetParam())
		{
		case allocation_type::malloc_free:
			free(p_);
			break;
		case allocation_type::new_delete:
		default:
			delete p_;
			break;
		}

		p_ = nullptr;
	}

	MemoryLeakDetectorListener detector;

private:
	void EndTest()
	{
		detector.OnTestEnd(*instance().current_test_info());
	}

	testing::UnitTest& instance() { return *testing::UnitTest::GetInstance(); }

	bool expect_memory_leak_;
	int* p_;
};

TEST_P(memory_leak_detector_test, 
    no_leak_should_be_detected__if_freeing_previously_allocated_memory_with_delete)
{
	expect_memory_leak_failure(false);
	allocate_memory();
	free_memory();
}

TEST_P(memory_leak_detector_test, 
    leak_should_be_detected__if_not_freeing_allocated_memory_with_new)
{
#ifndef NDEBUG // Only possible to test in debug build
	expect_memory_leak_failure(true);
	allocate_memory();
#endif
}

TEST_P(memory_leak_detector_test, 
    leak_should_be_detected__if_not_freeing_allocated_memory_with_malloc)
{
#ifndef NDEBUG // Only possible to test in debug build
	expect_memory_leak_failure(true);
	allocate_memory();
#endif
}

INSTANTIATE_TEST_CASE_P(various_allocation_methods, memory_leak_detector_test, ::testing::Values(
	allocation_type::new_delete,
	allocation_type::malloc_free
));

