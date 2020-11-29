// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file
// found in the root directory of this distribution.

#ifndef GTEST_MEMLEAK_DETECTOR_TEST_H
#define GTEST_MEMLEAK_DETECTOR_TEST_H

#include <gtest_memleak_detector/gtest_memleak_detector.h>
#include <gtest/gtest-spi.h> // enables testing test failures

using namespace gtest_memleak_detector;

enum class expected_outcome
{
    no_mem_leak,
    mem_leak_failure
};

class memory_leak_detector_listener_test : public ::testing::Test
{
public:
    memory_leak_detector_listener_test();
    virtual ~memory_leak_detector_listener_test() noexcept = default;
    memory_leak_detector_listener_test(const memory_leak_detector_listener_test&) = delete;
    memory_leak_detector_listener_test& operator=(const memory_leak_detector_listener_test&) = delete;
    memory_leak_detector_listener_test(memory_leak_detector_listener_test&&) = delete;
    memory_leak_detector_listener_test& operator=(memory_leak_detector_listener_test&&) = delete;

    void SetUp() override;
    void TearDown() override;

    void GivenPreTestSequence();
    void GivenPostTestSequence(expected_outcome action, 
        const char* failure_message = "Memory leak detected");

protected:
    MemoryLeakDetectorListener detector;

private:
    void EndTest();
    static testing::UnitTest& instance();
};

#endif // GTEST_MEMLEAK_DETECTOR_TEST_H