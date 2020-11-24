// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#ifndef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H
#define GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H

#include <gtest_memleak_detector/gtest_memleak_detector.h>

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

///////////////////////////////////////////////////////////////////////////////
// _CrtMemBlockHeader
//
// This normally opaque struct is vaguely documented here:
// https://docs.microsoft.com/en-us/visualstudio/debugger/crt-debug-heap-details?view=vs-2019
//
// Note that we are only interested in request counter so other fields are only
// relevant for getting padding right.
///////////////////////////////////////////////////////////////////////////////

typedef struct _CrtMemBlockHeader
{
    struct _CrtMemBlockHeader * pBlockHeaderNext;
    struct _CrtMemBlockHeader * pBlockHeaderPrev;
    char *                      szFileName;
    int                         nLine;
#ifdef _WIN64
    // These items are reversed on Win64 to eliminate gaps in the struct
    // and ensure that sizeof(struct)%16 == 0, so 16-byte alignment is
    // maintained in the debug heap.
    int                         nBlockUse;
    size_t                      nDataSize;
#else  /* _WIN64 */
    size_t                      nDataSize;
    int                         nBlockUse;
#endif  /* _WIN64 */
    long                        lRequest;
    // Further members not necessary
} _CrtMemBlockHeader;

struct trace_pointers
{
    char* buffer_ptr = nullptr;
    char* buffer_end_ptr = nullptr;
    char* filename_ptr = nullptr;
    unsigned long* line_ptr = nullptr;
};

namespace gtest_memleak_detector {

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
    StackTrace(char* buffer, char* buffer_end, char* filename, unsigned long* line, const char* hook_file);

    const char* Value() const noexcept;

protected:
    static void Copy(char* dst, const char* src) noexcept;

    bool Filter(const CallstackEntry& entry) noexcept;
    void Format(CallstackEntry& entry);
    virtual void OnCallstackEntry(
        CallstackEntryType eType, CallstackEntry& entry) override;
    virtual void OnDbgHelpErr(
        LPCSTR szFuncName, DWORD gle, DWORD64 addr) override;

private:
    char*  buffer;
    char*  buffer_last;
    char*  buffer_end;
    char*  filename;
    const char* hook_file;
    DWORD* line;
    bool   suppress;
    bool   cpp_trace_found;
    bool   next_cpp_trace_found;
    State  state;
};

///////////////////////////////////////////////////////////////////////////////
// MemoryLeakDetectorListener::Impl
///////////////////////////////////////////////////////////////////////////////

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