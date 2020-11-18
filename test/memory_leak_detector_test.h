// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file
// found in the root directory of this distribution.

#ifndef GTEST_MEMLEAK_DETECTOR_TEST_H
#define GTEST_MEMLEAK_DETECTOR_TEST_H

#include <gtest_memleak_detector/gtest_memleak_detector.h>

#include <gtest/gtest-spi.h> // enables testing test failures

using namespace gtest_memleak_detector;

enum class allocation_type
{
    new_delete,
    malloc_free,
    heap_alloc_free
};

enum class expected_outcome
{
    no_mem_leak,
    mem_leak_failure
};

class memory_leak_detector_test : public ::testing::Test
{
public:
    memory_leak_detector_test();
    virtual ~memory_leak_detector_test() noexcept = default;

    virtual void SetUp();
    virtual void TearDown();

    void GivenPreTestSequence();
    void GivenPostTestSequence(expected_outcome action);

    void GivenMemoryAllocated(allocation_type method);
    void GivenMemoryFreed(allocation_type method);

    MemoryLeakDetectorListener detector;

private:
    void EndTest();
    testing::UnitTest& instance();

    bool expect_memory_leak_;
    int* p_;
};

#endif // GTEST_MEMLEAK_DETECTOR_TEST_H