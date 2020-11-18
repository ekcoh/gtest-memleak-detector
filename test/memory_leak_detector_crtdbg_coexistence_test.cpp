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

        alloc_hook_installed = false;
        report_hook_installed = false;
        alloc_hook_call_count = 0u;
        report_hook_call_count = 0u;
    }

    virtual void TearDown()
    {
        GivenReportHookUninstalled();

        memory_leak_detector_test::TearDown();
    }

    unsigned AllocHookCallCount() const noexcept
    {
        return alloc_hook_call_count;
    }

    unsigned ReportHookCallCount() const noexcept
    {
        return report_hook_call_count;
    }

protected:
    void GivenAllocHookInstalled()
    {
        if (!alloc_hook_installed)
        {
            stored_alloc_hook = _CrtSetAllocHook(AllocHook);
            //ASSERT_EQ(_CrtSetAllocHook(AllocHook), nullptr);
            alloc_hook_installed = true;
        }
    }

    void GivenAllocHookUninstalled()
    {
        if (alloc_hook_installed)
        {
#pragma warning( push )            
#pragma warning( disable : 5039 )  // stored_alloc_hook will not throw
            _CrtSetAllocHook(stored_alloc_hook); // restore
#pragma warning( pop )
            //ASSERT_NE(_CrtSetAllocHook(nullptr), &AllocHook);
            alloc_hook_installed = false;
        }
    }

    void GivenReportHookInstalled()
    {
        if (!report_hook_installed)
        {
            ASSERT_EQ(_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, ReportHook), 1);
            report_hook_installed = true;
        }
    }

    void GivenReportHookUninstalled()
    {
        if (report_hook_installed)
        {
            ASSERT_EQ(_CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, ReportHook), 0);
            report_hook_installed = false;
        }
    }

private:
    static unsigned alloc_hook_call_count;
    static unsigned report_hook_call_count;
    
    static int AllocHook(int nAllocType, void* pvData,
        size_t nSize, int nBlockUse, long lRequest,
        const unsigned char* szFileName, int nLine) noexcept
    {
        (void)nAllocType;
        (void)pvData;
        (void)nSize;
        (void)nBlockUse;
        (void)lRequest;
        (void)szFileName;
        (void)nLine;

        ++alloc_hook_call_count;
        return 1; // TRUE - indicates success
    }

    static int ReportHook(int reportType, char* message, int* returnValue) noexcept
    {
        (void)reportType;
        (void)message;
        (void)returnValue;

        ++report_hook_call_count;
        return 1; // TRUE - indicates success
    }

    bool report_hook_installed;
    bool alloc_hook_installed;
    _CRT_ALLOC_HOOK stored_alloc_hook;
};

unsigned memory_leak_detector_crtdbg_coexistence_test::alloc_hook_call_count = 0u;
unsigned memory_leak_detector_crtdbg_coexistence_test::report_hook_call_count = 0u;

TEST_F(memory_leak_detector_crtdbg_coexistence_test,
    preinstalled_report_hooks__should_receive_callbacks__when_detector_is_running)
{
    GivenReportHookInstalled();

    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::new_delete);
    _CrtDumpMemoryLeaks(); // trigger report hook
    GivenMemoryFreed(allocation_type::new_delete);
    GivenPostTestSequence(expected_outcome::no_mem_leak);
    EXPECT_GT(ReportHookCallCount(), 1u); // actually ~250 but good enough

    GivenReportHookUninstalled();
}

TEST_F(memory_leak_detector_crtdbg_coexistence_test,
    preinstalled_alloc_hook__should_receive_callbacks__when_detector_is_running)
{
    GivenAllocHookInstalled();

    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::new_delete);
    _CrtDumpMemoryLeaks(); // trigger report hook
    GivenMemoryFreed(allocation_type::new_delete);
    GivenPostTestSequence(expected_outcome::no_mem_leak);
    EXPECT_GT(AllocHookCallCount(), 1u); // actually ~29 but good enough

    GivenAllocHookUninstalled();
}

// TODO Verify that preinstalled_alloc_hook receive callbacks when detector is running

TEST_F(memory_leak_detector_crtdbg_coexistence_test,
    leak_should_be_detected__even_if_there_are_preinstalled_report_hooks__when_detector_is_running)
{
    GivenReportHookInstalled();

    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::new_delete);
    _CrtDumpMemoryLeaks(); // trigger report hook
    GivenPostTestSequence(expected_outcome::mem_leak_failure);
    GivenMemoryFreed(allocation_type::new_delete);
    EXPECT_GT(ReportHookCallCount(), 1u); // actually ~250 but good enough

    GivenReportHookUninstalled();
}

TEST_F(memory_leak_detector_crtdbg_coexistence_test,
    leak_should_be_detected__even_if_there_are_report_hooks_installed_during_test__when_detector_is_running)
{
    GivenPreTestSequence();
    GivenMemoryAllocated(allocation_type::new_delete);
    GivenReportHookInstalled();
    _CrtDumpMemoryLeaks(); // trigger report hook
    GivenReportHookUninstalled();
    GivenPostTestSequence(expected_outcome::mem_leak_failure);
    GivenMemoryFreed(allocation_type::new_delete);
    EXPECT_GT(ReportHookCallCount(), 1u); // actually ~250 but good enough
}

#endif // defined(_DEBUG) && defined(_MSC_VER)

