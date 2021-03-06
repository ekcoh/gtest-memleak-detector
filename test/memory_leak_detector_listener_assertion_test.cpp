// Copyright(C) 2019 - 2020 H�kan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#include <gtest_memleak_detector/gtest_memleak_detector.h>

#include <memory>

#include "memory_leak_detector_listener_test.h" // fixture

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#pragma warning( push )
// warning C5039: potentially throwing function passed to extern C function 
// under -EHc. May result in undefined behavior.
#pragma warning( disable : 5039 ) 
#include <Windows.h>
#pragma warning( pop )

// No leak test cases

TEST_F(memory_leak_detector_listener_test, 
    no_leak_should_be_detected__if_freeing_previously_allocated_memory_before_test_end_with_new_delete)
{
    GivenPreTestSequence();
    auto* ptr = new int;
    delete ptr;
    GivenPostTestSequence(expected_outcome::no_mem_leak);
}

TEST_F(memory_leak_detector_listener_test,
    no_leak_should_be_detected__if_freeing_previously_allocated_memory_before_test_end_with_malloc_free)
{
    GivenPreTestSequence();
    auto* ptr = malloc(sizeof(double));
    free(ptr);
    GivenPostTestSequence(expected_outcome::no_mem_leak);
}

TEST_F(memory_leak_detector_listener_test,
    no_leak_should_be_detected__if_freeing_previously_allocated_memory_before_test_end_with_heap_alloc_free)
{
    GivenPreTestSequence();
    auto* ptr = HeapAlloc(GetProcessHeap(), 0, 64);
    HeapFree(GetProcessHeap(), 0, ptr);
    GivenPostTestSequence(expected_outcome::no_mem_leak);
}

// Leaking test cases

#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE

TEST_F(memory_leak_detector_listener_test,
    leak_should_be_detected__if_not_freeing_previously_allocated_memory_before_test_end_with_new_delete)
{
    GivenPreTestSequence();
    auto* ptr = new int;
    GivenPostTestSequence(expected_outcome::mem_leak_failure);
    delete ptr; // clean-up
}

TEST_F(memory_leak_detector_listener_test,
    leak_should_be_detected__if_not_freeing_previously_allocated_memory_before_test_end_with_malloc_free)
{
    GivenPreTestSequence();
    auto* ptr = malloc(32);
    GivenPostTestSequence(expected_outcome::mem_leak_failure);
    free(ptr); // clean-up
}

TEST_F(memory_leak_detector_listener_test,
    leak_should_not_be_detected__if_not_freeing_previously_allocated_memory_before_test_end_with_heap_alloc_free)
{
    // Note that HeapAlloc is not subject for CRTDBG
    GivenPreTestSequence();
    auto* ptr = HeapAlloc(GetProcessHeap(), 0, 32);
    GivenPostTestSequence(expected_outcome::no_mem_leak);
    HeapFree(GetProcessHeap(), 0, ptr); // clean-up
}

#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE




