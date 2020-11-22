// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

// Example 01 - Getting started
//
// Basic Google Test example showcasing some test cases leaking memory. Run the tests to see that
// the memory leaks are detected by the memory listener and reported as test failures.

#include <gtest/gtest.h>
#include <gtest_memleak_detector/gtest_memleak_detector.h>

// This is typically defined in its own file, but defined here to reduce complexity of example
int main(int argc, char **argv)
{
    // --gtest_filter=example_01_memory_leak_detection.forgetting_to_cleanup_allocation_with_malloc_will_leak_memory
    // --gtest_filter=example_01_memory_leak_detection.forgetting_to_cleanup_allocation_with_new_will_leak_memory:example_01_memory_leak_detection.forgetting_to_cleanup_allocation_with_malloc_will_leak_memory

    for (int i = 0; i < argc; ++i)
    {
        const char* arg = argv[i];
        std::cout << i << ":" << argv[i] << "\n";
    }

	::testing::InitGoogleTest(&argc, argv);
	::testing::UnitTest::GetInstance()->listeners().Append(
		new gtest_memleak_detector::MemoryLeakDetectorListener(argc, argv));
	return RUN_ALL_TESTS();
}

TEST(example_01_memory_leak_detection,
	proper_memory_management_with_raii_cleanup_will_not_leak_memory)
{   // unique is deallocated when ptr goes out of scope 
    // (no memory leak)
	auto ptr = std::make_unique<int>(5);
	EXPECT_EQ(*ptr, 5);
}

TEST(example_01_memory_leak_detection,
	memory_leak_will_not_be_reported_if_there_is_assertion_failures)
{   // ptr is never deallocated and will leak, but will not be 
    // reported since EXPECT_EQ also fails and should be fixed first
    // (suppressed leak report)
	auto ptr = new int(5);
	EXPECT_EQ(*ptr, 6);
}

TEST(example_01_memory_leak_detection,
    in_case_of_multiple_memory_leaks_only_the_first_leak_is_reported)
{
    // ptr_1 and ptr_2 are never deallocated and will leak
    // (multiple - only first one reported)
    auto ptr_1 = new int(5);
    auto ptr_2 = new int(7); // will not be reported
    EXPECT_EQ(*ptr_1, 5);
    EXPECT_EQ(*ptr_2, 7);
}

TEST(example_01_memory_leak_detection,
	forgetting_to_cleanup_allocation_with_new_will_leak_memory)
{
    // ptr is never deallocated and will leak
    // (memory leak reported)
	auto ptr = new int(5);
	EXPECT_EQ(*ptr, 5);
}

TEST(example_01_memory_leak_detection,
	forgetting_to_cleanup_allocation_with_malloc_will_leak_memory)
{
    // ptr is never freed and will leak
    // (memory leak reported)
	auto ptr = static_cast<int*>(malloc(sizeof(int)));
	*ptr = 5;
	EXPECT_EQ(*ptr, 5);
}