// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#include <gtest_memleak_detector/gtest_memleak_detector.h>

#include <memory>

#include "memory_leak_detector_test.h" // fixture

TEST_F(memory_leak_detector_test, 
    no_leak_should_be_detected__if_freeing_previously_allocated_memory_before_test_end_with_new_delete)
{
    GivenPreTestSequence();
	GivenMemoryAllocated(allocation_type::new_delete);
	GivenMemoryFreed(allocation_type::new_delete);
    GivenPostTestSequence(expected_outcome::no_mem_leak);
}

TEST_F(memory_leak_detector_test,
    no_leak_should_be_detected__if_freeing_previously_allocated_memory_before_test_end_with_malloc_free)
{
    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::malloc_free);
    GivenMemoryFreed(allocation_type::malloc_free);
    GivenPostTestSequence(expected_outcome::no_mem_leak);
}

TEST_F(memory_leak_detector_test,
    leak_should_be_detected__if_not_freeing_previously_allocated_memory_before_test_end_with_new_delete)
{
#ifndef NDEBUG // Only possible to test in debug build
    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::new_delete);
    GivenPostTestSequence(expected_outcome::mem_leak_failure);
    GivenMemoryFreed(allocation_type::new_delete); // clean-up
#endif
}

TEST_F(memory_leak_detector_test,
    leak_should_be_detected__if_not_freeing_previously_allocated_memory_before_test_end_with_malloc_free)
{
#ifndef NDEBUG // Only possible to test in debug build
    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::malloc_free);
    GivenPostTestSequence(expected_outcome::mem_leak_failure);
    GivenMemoryFreed(allocation_type::malloc_free); // clean-up
#endif
}



