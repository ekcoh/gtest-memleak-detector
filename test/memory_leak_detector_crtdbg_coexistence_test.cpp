// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

// Memory debugging tools (MSVC only)
#if defined(_DEBUG) && defined(_MSC_VER)

// CRT debug tools
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include <gtest_memleak_detector/gtest_memleak_detector.h>

#include "memory_leak_detector_test.h"

using namespace gtest_memleak_detector;

class memory_leak_detector_crtdbg_coexistence_test : public memory_leak_detector_test
{
public:
    virtual void SetUp()
    {
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

        memory_leak_detector_test::SetUp();

        hook_installed = false;
        report_hook_call_count = 0u;
    }

    virtual void TearDown()
    {
        GivenHookUninstalled();

        memory_leak_detector_test::TearDown();
    }

    unsigned ReportHookCallCount() const noexcept
    {
        return report_hook_call_count;
    }

protected:
    void GivenHookInstalled()
    {
        if (!hook_installed)
        {
            ASSERT_EQ(_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, ReportHook), 1);
            hook_installed = true;
        }
    }

    void GivenHookUninstalled()
    {
        if (hook_installed)
        {
            ASSERT_EQ(_CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, ReportHook), 0);
            hook_installed = false;
        }
    }

private:
    static unsigned report_hook_call_count;
    
    static int ReportHook(int /*reportType*/, char* /*message*/, int* /*returnValue*/) noexcept
    {
        ++report_hook_call_count;
        return 1; // TRUE
    }

    bool hook_installed;
};

unsigned memory_leak_detector_crtdbg_coexistence_test::report_hook_call_count = 0u;

TEST_F(memory_leak_detector_crtdbg_coexistence_test,
    preinstalled_report_hooks__should_receive_callbacks__when_detector_is_running)
{
    GivenHookInstalled();

    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::new_delete);
    _CrtDumpMemoryLeaks(); // trigger report hook
    GivenMemoryFreed(allocation_type::new_delete);
    GivenPostTestSequence(expected_outcome::no_mem_leak);
    EXPECT_GT(ReportHookCallCount(), 1u); // actually ~250 but good enough

    GivenHookUninstalled();
}

TEST_F(memory_leak_detector_crtdbg_coexistence_test,
    leak_should_be_detected__even_if_there_are_preinstalled_report_hooks__when_detector_is_running)
{
    GivenHookInstalled();

    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::new_delete);
    _CrtDumpMemoryLeaks(); // trigger report hook
    GivenPostTestSequence(expected_outcome::mem_leak_failure);
    GivenMemoryFreed(allocation_type::new_delete);
    EXPECT_GT(ReportHookCallCount(), 1u); // actually ~250 but good enough

    GivenHookUninstalled();
}

TEST_F(memory_leak_detector_crtdbg_coexistence_test,
    leak_should_be_detected__even_if_there_are_report_hooks_installed_during_test__when_detector_is_running)
{
    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::new_delete);
    GivenHookInstalled();
    _CrtDumpMemoryLeaks(); // trigger report hook
    GivenHookUninstalled();
    GivenPostTestSequence(expected_outcome::mem_leak_failure);
    GivenMemoryFreed(allocation_type::new_delete);
    EXPECT_GT(ReportHookCallCount(), 1u); // actually ~250 but good enough
}

#endif // defined(_DEBUG) && defined(_MSC_VER)

