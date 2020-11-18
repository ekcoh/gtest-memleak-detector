// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#ifndef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H

#include <gtest_memleak_detector/gtest_memleak_detector.h>

// Memory debugging tools (MSVC only)
#if defined(_DEBUG) && defined(_MSC_VER)
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE
	
// CRT debug tools
#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif // _CRTDBG_MAP_ALLOC

#include <crtdbg.h>      // _CrtMemState
#include <fstream>       // std::ifstream, std::ofstream
#include <unordered_map> // std::unordered_map

namespace gtest_memleak_detector
{
    class MemoryLeakDetectorListener::Impl
    {
    public:
	    Impl(int argc, char** argv0);
	    ~Impl() noexcept = default;

        const Impl(const Impl&) = delete;
        const Impl(Impl&&) noexcept = delete;
        Impl& operator=(const Impl&) = delete;
        Impl& operator=(Impl&&) = delete;

	    void OnTestProgramStart(const ::testing::UnitTest& unit_test);
	    void OnTestStart(const ::testing::TestInfo& test_info);
	    void OnTestEnd(const ::testing::TestInfo& test_info);
	    void OnTestProgramEnd(const ::testing::UnitTest& unit_test);
	
    private:
        static int AllocHook(int nAllocType, void* pvData,
            size_t nSize, int nBlockUse, long lRequest,
            const unsigned char* szFileName, int nLine) noexcept;

        static bool TryParseAllocNo(long& dst, const char* str) noexcept;

        bool ReadAndCompare(int argc, char** argv);
        
        void SetAllocHook();
        void RevertAllocHook();

        void Fail();

	    std::ifstream   in_;
	    std::ofstream   out_;
        _CrtMemState    pre_state_;
        bool            alloc_hook_set_;
        //long            break_alloc_;
        struct _stat    file_info_;
        char            buffer[GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH]{ 0 };
        char            filename[GTEST_MEMLEAK_DETECTOR_PATH_MAX_LENGTH]{ 0 };
        unsigned long   line;
        std::string     temp_file_path;
        std::string     file_path;
        std::unordered_map<std::string, long> db;
    };
} // namespace gtest_memleak_detector

#else

#ifdef _MSC_VER
#pragma message ( \
	"WARNING: Memory leak detection not supported by this compiler/configuration/" \
	"platform combination. All memory leak assertions will be disabled. " \
	"This may be caused by a non-debug build, e.g. release build.")
#endif // _MSC_VER

namespace gtest_memleak_detector
{
	class MemoryLeakDetectorListener::Impl { };
} // namespace gtest_memleak_detector

#endif // defined(_DEBUG) && defined(_MSC_VER)

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H