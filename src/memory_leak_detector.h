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
#ifdef _M_IX86
#define GTEST_MEMLEAK_DETECTOR_M_IX86_DEFINED
#undef _M_IX86 // x86-64 work-around for StackWalker x86 build problem
#endif
#include <StackWalker/StackWalker.h>
#ifdef GTEST_MEMLEAK_DETECTOR_M_IX86_DEFINED
#define _M_IX86 // restore
#endif
#pragma warning( pop )
#ifdef GTEST_MEMLEAK_DETECTOR_WIN32_LEAN_AND_MEAN_DEFINED
#undef WIN32_LEAN_AND_MEAN // cleanup
#endif // WIN32_LEAN_AND_MEAN

// CRT debug tools
#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif // _CRTDBG_MAP_ALLOC

#include <cstdio>        // snprintf_s
#include <crtdbg.h>      // _CrtMemState
#include <fstream>       // std::ifstream, std::ofstream
#include <unordered_map> // std::unordered_map
#include <sstream>       // std::stringstream

// Internal debugging:
// Uncomment to debug during development of this library
// Note that a fixed memory buffer is used and size might need adjustment
// #define GTEST_MEMLEAK_DETECTOR_DEBUG

// Internal debugging:
// Uncomment below to trace allocation hook callbacks
// #define GTEST_MEMLEAK_DETECTOR_DEBUG_TRACE_ALLOC

#ifdef GTEST_MEMLEAK_DETECTOR_DEBUG
#define GTEST_MEMLEAK_DETECTOR_DEBUG_BUFFER_SIZE_BYTES 640000
#endif 

namespace gtest_memleak_detector {

///////////////////////////////////////////////////////////////////////////////
// Location
///////////////////////////////////////////////////////////////////////////////

struct Location
{
    static constexpr unsigned long invalid_line = static_cast<unsigned long>(-1);

    unsigned long   line = invalid_line;
    std::string     file;

    inline void Clear() noexcept
    {
        line = invalid_line;
        file.clear();
    }

    inline bool Empty() noexcept
    {
        return line == invalid_line && file.empty();
    }
};

///////////////////////////////////////////////////////////////////////////////
// StackTrace
///////////////////////////////////////////////////////////////////////////////

class StackTrace final : public StackWalker
{
public:
    enum class State
    {
        Scanning,
        Capture,
        Completed,
        Exception
    };

    StackTrace();
    virtual ~StackTrace() = default;

    State CurrentState() const noexcept;
    const std::stringstream& Stream() const noexcept;
    const Location& GetLocation() const noexcept;
    void Reset(State reset_to_state = State::Scanning);

protected:
    bool Filter(const CallstackEntry& entry) noexcept;
    void Format(CallstackEntry& entry);
    void HandleCallstackEntry(CallstackEntry& entry);
    virtual void OnCallstackEntry(
        CallstackEntryType eType, CallstackEntry& entry) override;
    virtual void OnDbgHelpErr(
        LPCSTR szFuncName, DWORD gle, DWORD64 addr) override;

private:
    std::stringstream  buffer;
    Location           location;
    State              state;
};

///////////////////////////////////////////////////////////////////////////////
// MemoryLeakDetector
///////////////////////////////////////////////////////////////////////////////

class MemoryLeakDetector
{
public:
    static constexpr long no_break_alloc = -1;

#ifdef GTEST_MEMLEAK_DETECTOR_DEBUG
    static constexpr size_t debug_buffer_size =
        GTEST_MEMLEAK_DETECTOR_DEBUG_BUFFER_SIZE_BYTES;
#endif

    using FailureCallback = std::function<void(
        long leak_alloc_no,
        const char* leak_file,
        unsigned long leak_line,
        const char* leak_trace)>;

    struct State {
        long pre_alloc_no = 0;
        long post_alloc_no = 0;
        long pre_trace_no = 0;
        long post_trace_no = 0;
        long break_alloc = no_break_alloc;
        long parsed_alloc_no = no_break_alloc;
        bool discard = false;

#ifdef GTEST_MEMLEAK_DETECTOR_DEBUG
        char debug_buffer[debug_buffer_size]{ 0 };
        size_t debug_buffer_length = 0;
        size_t debug_buffer_truncated = 0;
#endif
    };

	explicit MemoryLeakDetector(int argc, char** argv0);
	~MemoryLeakDetector() noexcept = default;

    const MemoryLeakDetector(const MemoryLeakDetector&) = delete;
    const MemoryLeakDetector(MemoryLeakDetector&&) noexcept = delete;
    MemoryLeakDetector& operator=(const MemoryLeakDetector&) = delete;
    MemoryLeakDetector& operator=(MemoryLeakDetector&&) = delete;

    static MemoryLeakDetector* Instance() noexcept
    {
        assert(instance_);
        return instance_;
    }

	void Start(std::function<std::string()> descriptor);
	void End(std::function<std::string()> descriptor, bool passed);

    static std::string MakeDatabaseFilePath(const char* binary_file_path);
    static std::string MakeFailureMessage(long leak_alloc_no,
        const char* leak_file,
        unsigned long leak_line,
        const char* leak_trace);

    void WriteDatabase();
    void SetFailureCallback(FailureCallback callback);
    void SetTrace(const Location& location, std::string stack_trace);
    void OnAllocation(int nAllocType, long lRequest);
    void OnReport(const char* message) noexcept;

#ifdef GTEST_MEMLEAK_DETECTOR_DEBUG

    /*bool DebugBufferFull()
    {
        return state_.debug_buffer_length >= debug_buffer_size - 1;
    }*/

    template<size_t N, class... Args>
    void Log(const char(&fmt)[N], Args&&... args)
    {
        const auto m = state_.debug_buffer_length;
        if (m >= debug_buffer_size - 1)
        {   // Count number of line to give more informative truncation message
            auto rows = 1u;
            for (auto* p = strchr(fmt, '\n'); p != nullptr; p = strchr(p + 1, '\n'))
                ++rows;
            state_.debug_buffer_truncated += rows;
            return; // truncate, buffer already full 
        }

        auto* dst = state_.debug_buffer + m;
        const auto max_count = debug_buffer_size - m - 1;
        const auto n = snprintf(
            dst, 
            max_count, 
            fmt, 
            std::forward<Args>(args)...);
        if (n > 0)
            state_.debug_buffer_length = (std::min)(m + n, debug_buffer_size - 1);   
    }

    void ResetDebugBuffer() noexcept
    {
        state_.debug_buffer[0] = 0;
        state_.debug_buffer_length = 0;
        state_.debug_buffer_truncated = 0;
    }

    void DumpAndResetLog()
    {
        if (state_.debug_buffer_length > 0)
        {
            if (state_.debug_buffer_length >= debug_buffer_size - 1)
            {
                static constexpr char truncated[] = "\n[ truncated %zd lines ]\n";
                const auto n = snprintf(state_.debug_buffer, 0, truncated, state_.debug_buffer_truncated);
                snprintf(state_.debug_buffer + debug_buffer_size - 1 - n,
                    static_cast<size_t>(n + 1), truncated, state_.debug_buffer_truncated);
            }

            fprintf(stderr, "Debug Log (%zd):\n", state_.debug_buffer_length);
            fprintf(stderr, state_.debug_buffer);
        }
        ResetDebugBuffer();
    }

    void LogStackTrace();

#endif // GTEST_MEMLEAK_DETECTOR_DEBUG

private:
    void CaptureLeakStackTrace();

    bool ReadDatabase();
    bool TryReadDatabase();

    void SetAllocHook();
    void RevertAllocHook();

    using Database = std::unordered_map<std::string, long>;
    using ReRun = std::vector<std::string>;

    static MemoryLeakDetector* instance_;

    State             state_;
    _CrtMemState      pre_state_;
    int               stored_debug_flags_;
    bool              alloc_hook_set_;
    struct _stat      file_info_;
    std::string       trace_;
    size_t            hash_;
    Location          location_;
    std::string       file_path_;
    Database          db_;
    ReRun             rerun_filter_;
    FailureCallback   fail_;
#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    StackTrace        stack_trace_;
#endif
};

} // namespace gtest_memleak_detector

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_H