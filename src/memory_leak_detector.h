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

#pragma warning( push )
// warning C5039: potentially throwing function passed to extern C function 
// under -EHc. May result in undefined behavior.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // slightly decrease compile-time
#define GTEST_MEMLEAK_DETECTOR_WIN32_LEAN_AND_MEAN_DEFINED
#endif
#pragma warning( disable : 5039 ) 
#include <Windows.h>
#include <StackWalker/StackWalker.h>
#pragma warning( pop )
#ifdef GTEST_MEMLEAK_DETECTOR_WIN32_LEAN_AND_MEAN_DEFINED
#undef WIN32_LEAN_AND_MEAN // cleanup
#endif // WIN32_LEAN_AND_MEAN

// CRT debug tools
#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif // _CRTDBG_MAP_ALLOC

#include <crtdbg.h>      // _CrtMemState
#include <fstream>       // std::ifstream, std::ofstream
#include <unordered_map> // std::unordered_map

namespace gtest_memleak_detector {

///////////////////////////////////////////////////////////////////////////////
// Buffer
///////////////////////////////////////////////////////////////////////////////

struct Buffer
{
    char  data[GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH]{ 0 };
    char* last = nullptr;
    char* end = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// Location
///////////////////////////////////////////////////////////////////////////////

struct Location
{
    static constexpr unsigned long invalid_line = 
        static_cast<unsigned long>(-1);
    char            file[GTEST_MEMLEAK_DETECTOR_PATH_MAX_LENGTH]{ 0 };
    unsigned long   line = invalid_line;
};

///////////////////////////////////////////////////////////////////////////////
// StackTrace
///////////////////////////////////////////////////////////////////////////////

class StackTrace final : public StackWalker
{
private:
    static constexpr size_t max_path = GTEST_MEMLEAK_DETECTOR_PATH_MAX_LENGTH;
    
    enum class State
    {
        Pre,
        Active,
        Post
    };
public:
    StackTrace(Buffer* buffer, Location* location, const char* hook_file);

protected:
    static void Copy(char* dst, const char* src) noexcept;

    bool Filter(const CallstackEntry& entry) noexcept;
    void Format(CallstackEntry& entry);
    virtual void OnCallstackEntry(
        CallstackEntryType eType, CallstackEntry& entry) override;
    virtual void OnDbgHelpErr(
        LPCSTR szFuncName, DWORD gle, DWORD64 addr) override;

private:
    Buffer*     buffer;
    Location*   location;
    const char* hook_file;
    State       state;
    bool        suppress;
};

///////////////////////////////////////////////////////////////////////////////
// MemoryLeakDetector
///////////////////////////////////////////////////////////////////////////////

class MemoryLeakDetector
{
public:
    using FailureCallback = std::function<void(
        long leak_alloc_no,
        const char* leak_file,
        unsigned long leak_line,
        const char* leak_trace)>;

	explicit MemoryLeakDetector(int argc, char** argv0);
	~MemoryLeakDetector() noexcept = default;

    const MemoryLeakDetector(const MemoryLeakDetector&) = delete;
    const MemoryLeakDetector(MemoryLeakDetector&&) noexcept = delete;
    MemoryLeakDetector& operator=(const MemoryLeakDetector&) = delete;
    MemoryLeakDetector& operator=(MemoryLeakDetector&&) = delete;

	void Start(std::function<std::string()> descriptor);
	void End(std::function<std::string()> descriptor, bool passed);

    void SetFailureCallback(FailureCallback callback);

    static std::string MakeDatabaseFilePath(const char* binary_file_path);
    static std::string MakeFailureMessage(long leak_alloc_no,
        const char* leak_file,
        unsigned long leak_line,
        const char* leak_trace);

    static const char* database_file_suffix; // TODO Consider removing!?

    void WriteDatabase();
private:
    //static int AllocHook(int nAllocType, void* pvData,
    //    size_t nSize, int nBlockUse, long lRequest,
    //    const unsigned char* szFileName, int nLine) noexcept;

    static bool TryParseAllocNo(long& dst, const char* str) noexcept;
    //long ParseMemLeakAllocNo(_CrtMemState& mem_diff) const noexcept;
    
    bool ReadDatabase();
    bool TryReadDatabase();

    void SetAllocHook();
    void RevertAllocHook();

    _CrtMemState    pre_state_;
    int             stored_debug_flags_;
    bool            alloc_hook_set_;
    struct _stat    file_info_;
    Buffer          buffer;
    Location        location;
    std::string     file_path;
    std::unordered_map<std::string, long> db_;
    std::vector<std::string> rerun_filter_;
    FailureCallback fail_;
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