// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#ifndef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H

#include <gtest_memleak_detector/gtest_memleak_detector.h>

// Memory debugging tools (MSVC only)
#if defined(_DEBUG) && defined(_MSC_VER) && defined(_WIN32)
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE
	
// CRT debug tools
#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif // _CRTDBG_MAP_ALLOC

#include <crtdbg.h>      // _CrtMemState
#include <fstream>       // std::ifstream, std::ofstream
#include <unordered_map> // std::unordered_map

namespace gtest_memleak_detector {

class MemoryLeakDetectorListener::Impl
{
public:
	Impl(int argc, char** argv0);
	~Impl() noexcept = default;

    const Impl(const Impl&) = delete;
    const Impl(Impl&&) noexcept = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;

	void OnTestStart(std::function<std::string()> descriptor);
	void OnTestEnd(std::function<std::string()> descriptor, bool passed);

    //void SetFailureCallback(std::function<void(const char*)> callback);

    void WriteDatabase();
private:
    static int AllocHook(int nAllocType, void* pvData,
        size_t nSize, int nBlockUse, long lRequest,
        const unsigned char* szFileName, int nLine) noexcept;

    static bool TryParseAllocNo(long& dst, const char* str) noexcept;
    long ParseMemLeakAllocNo(_CrtMemState& mem_diff) const noexcept;
    
    bool ReadDatabase();
    bool TryReadDatabase();

    void SetAllocHook();
    void RevertAllocHook();

    _CrtMemState    pre_state_;
    int             stored_debug_flags_;
    bool            alloc_hook_set_;
    struct _stat    file_info_;
    char            buffer[GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH]{ 0 };
    char            filename[GTEST_MEMLEAK_DETECTOR_PATH_MAX_LENGTH]{ 0 };
    unsigned long   line;
    std::string     file_path;
    std::unordered_map<std::string, long> db_;
    std::vector<std::string> rerun_filter_;
    //std::function<void(const char* message)> fail_;
};

} // namespace gtest_memleak_detector

#else

#ifdef _MSC_VER
#pragma message ( \
	"WARNING: Memory leak detection not supported by this compiler/configuration/" \
	"platform combination. All memory leak assertions will be disabled. " \
	"This is expected for non-debug builds, e.g. release build.")
#endif // _MSC_VER

namespace gtest_memleak_detector
{
	class MemoryLeakDetectorListener::Impl { };
} // namespace gtest_memleak_detector

#endif // defined(_DEBUG) && defined(_MSC_VER) && defined(_WIN32)

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H