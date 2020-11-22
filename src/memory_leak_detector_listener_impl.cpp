// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#include "memory_leak_detector_listener_impl.h"

#include <string>   // std::string
#include <exception>

#pragma warning( push )
// warning C5039: potentially throwing function passed to extern C function 
// under -EHc. May result in undefined behavior.
#pragma warning( disable : 5039 ) 
#include <Windows.h>
#include <StackWalker/StackWalker.h>
#pragma warning( pop )

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE

// This normally opaque struct is vaguely documented here:
// https://docs.microsoft.com/en-us/visualstudio/debugger/crt-debug-heap-details?view=vs-2019
//
// Note that we are only interested in request counter so other fields are only
// relevant for getting padding right.
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

// Constants
constexpr long no_break_alloc = -1;

// Locals
_CRT_ALLOC_HOOK stored_alloc_hook = nullptr;
trace_pointers  pointers;
//long            parsed_alloc_no = no_break_alloc;
long            break_alloc = no_break_alloc;
bool            discard = false;

long            alloc_no = 0;
long            pre_alloc_no = 0;
long            post_alloc_no = 0;
long            pre_trace_no = 0;
long            post_trace_no = 0;

// Translation unit utility functions
namespace {

///////////////////////////////////////////////////////////////////////////////
// Trace declaration - Stack trace formatting
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
    StackTrace(char* buffer, char* buffer_end, char* filename, unsigned long* line);

    inline const char* Value() const noexcept
    {
        assert(buffer_last);
        assert(buffer <= buffer_last && buffer_last < buffer_end);
        *(buffer_last) = '\0'; // terminate string in case of truncated result
        return buffer;
    }

protected:
    static void Copy(char* dst, const char* src) noexcept
    {
        assert(dst);
        assert(src);
        static_assert(_MSC_VER > 1400, "Requires MSVC version 2008 or newer");
        (void)strncpy_s(dst, STACKWALK_MAX_NAMELEN, src, _TRUNCATE);
    }

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
    DWORD* line;
    bool   suppress;
    bool   cpp_trace_found;
    bool   next_cpp_trace_found;
    State  state;
};

///////////////////////////////////////////////////////////////////////////////
// Trace definitions
///////////////////////////////////////////////////////////////////////////////

StackTrace::StackTrace(char* buffer, char* buffer_end, char* filename, unsigned long* line)
    : StackWalker(StackWalker::StackWalkOptions::RetrieveLine)
    , buffer(buffer)
    , buffer_last(buffer)
    , buffer_end(buffer_end)
    , filename(filename)
    , line(line)
    , suppress(true)
    , cpp_trace_found(false)
    , next_cpp_trace_found(false)
    , state(State::Pre)
{
    memset(buffer, 0, sizeof(sizeof(buffer) / sizeof(char)));
}

bool StackTrace::Filter(const CallstackEntry& entry) noexcept
{
    // Provide some custom VC filtering to prettify stacktrace
    if (entry.undName[0] != 0)
    {
        if (entry.lineFileName[0] == 0)
        {
            if (strcmp(entry.undName, "calloc_base") == 0)
                return true;
            if (strcmp(entry.undName, "malloc_dbg") == 0)
                return true;
            if (strcmp(entry.undName, "malloc") == 0)
                return true;
        }
        if (strcmp(entry.undName, "operator new") == 0)
            return true;
    }
    return false;
}

void StackTrace::Format(CallstackEntry& entry)
{
    const size_t maxCount = static_cast<size_t>(buffer_end - buffer_last - 1);
    int offset = 0;
    if (entry.name[0] == 0)
        Copy(entry.name, "(function-name not available)");
    if (entry.undName[0] != 0)
        Copy(entry.name, entry.undName);
    if (entry.undFullName[0] != 0)
        Copy(entry.name,  entry.undFullName);
    if (entry.lineFileName[0] == 0)
    {
        Copy(entry.lineFileName, "(filename not available)");
        if (entry.moduleName[0] == 0)
            Copy(entry.moduleName, "(module-name not available)");
        offset = _snprintf_s(buffer_last, maxCount, maxCount, "- 0x%p (%s): %s: %s\n",
            (LPVOID)entry.offset, entry.moduleName, entry.lineFileName, entry.name);
    }
    else
    {
        offset = _snprintf_s(buffer_last, maxCount, maxCount, "- %s (%lu): %s\n",
            entry.lineFileName, entry.lineNumber, entry.name);
    }
    if (offset > 0)
        buffer_last += offset;
}

void StackTrace::OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr)
{   // Discard dbg help error message, this is expected for e.g. malloc/realloc/free
    (void)(szFuncName);
    (void)(gle);
    (void)(addr);
}

void StackTrace::OnCallstackEntry(CallstackEntryType eType, CallstackEntry& entry) 
{
    if (eType == lastEntry || entry.offset == 0)
        return;

    switch (state)
    {
    case State::Pre:
        // First stack trace hit of relevance is AllocHook
        if (_stricmp(entry.lineFileName, __FILE__) == 0)
            state = State::Active;
        break;
    case State::Post:
        break;
    case State::Active:
        if (entry.undName[0] != 0)
        {
            // Abort if filtered stack trace
#ifndef GTEST_MEMLEAK_DETECTOR_DISABLE_FILTER_STACKTRACE
            if (Filter(entry))
                return;
#endif // GTEST_MEMLEAK_DETECTOR_DISABLE_FILTER_STACKTRACE

            // Store first stack trace row as origin
            if (filename[0] == 0 && entry.lineFileName[0] != 0)
            {
                strncpy_s(filename, max_path,
                    entry.lineFileName, max_path - 1);
                filename[max_path - 1] = 0;
                *line = entry.lineNumber;
            }

            // Truncate end of stack trace based on hitting test
            const auto len = strlen(entry.undName);
            const auto end = entry.undName + len - 10;
            if (len >= 10 && memcmp(end, "::TestBody", 10) == 0)
                state = State::Post;
        }

        Format(entry);
        break;

    default:
        break;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////



// Failing a test will by itself trigger a memory allocation... is this deterministic?! 
// Investigate if allocations can be predicted...

//char* describe_test2(char* buffer, char* buffer_end, 
//    const ::testing::TestInfo& test_info)
//{
//    using traits = std::char_traits<char>;
//    
//    const auto length = strlen(test_info.test_suite_name());
//    if (length > (buffer_end - buffer - 1))
//        return nullptr;
//    traits::copy(buffer, test_info.test_suite_name(), length);
//    buffer += length;
//
//    if ((buffer_end - buffer - 1) > 2)
//    *buffer++ = ':';
//    *buffer++ = ':';
//    
//}

} // anonomuous namespace

///////////////////////////////////////////////////////////////////////////////
// gtest_memleak_detector definitions
///////////////////////////////////////////////////////////////////////////////



bool gtest_memleak_detector::MemoryLeakDetectorListener::Impl::TryParseAllocNo(
    long& dst, const char* str) noexcept
{   // IMPORTANT: This function must have noexcept/nothrow semantics
    //            since indirectly called by C-run-time.
    if (!str)
        return false; // nullptr

    const auto start = strchr(str, '{');
    if (nullptr == start)
        return false; // failed (format error)

    const auto stop = strchr(start, '}');
    if (nullptr == stop)
        return false; // failed (format error)

    const auto len = stop - start - 1;
    if (len <= 0 || len > 10)
    {   // alloc_no is 32-bit (long) on Windows
        return false; // failed (range error)
    }

    // Parse allocation no as 32-bit integer.
    char buffer[11]; // 10 + termination char
    strncpy_s(buffer, start + 1, static_cast<size_t>(len));
    const auto allocation_no = strtol(buffer, nullptr, 10);
    if (allocation_no == LONG_MIN || allocation_no == LONG_MAX)
        return false; // failed

    dst = allocation_no;
    return true; // success
}

gtest_memleak_detector::MemoryLeakDetectorListener::Impl::Impl(int argc, char** argv) :
	pre_state_{ 0 },
    stored_debug_flags_(0),
    alloc_hook_set_(false)
{
    // Require binary path as first argument
    if (argc == 0)
        throw std::exception("at least executable name required");
    if (argv == nullptr)
        throw std::exception("missing command line arguments");

    // Set database file path
    file_path = MakeDatabaseFilePath(argv[0]);

    // Get test binary file info
    if (_stat(argv[0], &file_info_) == 0) // TODO THROW instead?!
    {
        if (!ReadDatabase())
            std::remove(file_path.c_str());
    }
    
	// Turn on debug allocation
	stored_debug_flags_ = _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); 
}

bool gtest_memleak_detector::MemoryLeakDetectorListener::Impl::ReadDatabase()
{
    std::ifstream in;
    in.open(file_path);
    if (!in)
        return true; // file do not exist

    // Parse file size
    decltype(file_info_.st_size) st_size = 0;
    in >> st_size;
    if (st_size != file_info_.st_size)
        return false; // different file size

    // Parse last modification timestamp
    decltype(file_info_.st_mtime) st_mtime = 0;
    in >> st_mtime;
    if (st_mtime != file_info_.st_mtime)
        return false; // different modification data

    // Allocate capacity for all elements to reduce allocation count
    size_t size;
    in >> size;
    db_.reserve(size);

    // Parse {description, leak_alloc_no} pairs
    std::string name;
    long leak_alloc_no;
    for (auto i = 0u; i < size; ++i)
    {
        in >> name;
        in >> leak_alloc_no;

        db_.emplace(name, leak_alloc_no);
    }

    return true; // success
}

bool gtest_memleak_detector::MemoryLeakDetectorListener::Impl::TryReadDatabase()
{
    try
    {
        ReadDatabase();
    }
    catch (std::bad_alloc&)
    {
        throw;      // re-throw
    }
    catch (...)
    {
        return false; // corrupt or incompatible file
    }
    
    return true; // successfully parsed all data
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::WriteDatabase()
{
    std::ofstream out;
    out.open(file_path);
    out << file_info_.st_size << '\n'
        << file_info_.st_mtime << '\n'
        << db_.size() << '\n';
    for (auto& kvp : db_)
        out << kvp.first << '\n' << kvp.second << '\n';
    out.flush();
    out.close();
}

int gtest_memleak_detector::MemoryLeakDetectorListener::Impl::AllocHook(
    int nAllocType, void* pvData,
    size_t nSize, int nBlockUse, long lRequest,
    const unsigned char* szFileName, int nLine) noexcept
{
    UNREFERENCED_PARAMETER(pvData);
    UNREFERENCED_PARAMETER(nSize);
    UNREFERENCED_PARAMETER(nBlockUse);
    UNREFERENCED_PARAMETER(szFileName);
    UNREFERENCED_PARAMETER(nLine);

    if (nAllocType != _HOOK_FREE)
    {
        if (nAllocType != _HOOK_REALLOC)
            alloc_no = lRequest;
    }

    if (discard)
    {   

    }
    else
    {
        switch (nAllocType)
        {
        case _HOOK_ALLOC:
            //sprintf_s(buf, "ALLOC request_=%ld\n", lRequest);
            //OutputDebugStringA(buf);
            if (lRequest == break_alloc)
            {
                discard = true;
                pre_trace_no = alloc_no;
                StackTrace trace(pointers.buffer_ptr,
                    pointers.buffer_ptr + GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH,
                    pointers.filename_ptr, pointers.line_ptr);
                trace.ShowCallstack(); // Result stored in buffer_ptr
                post_trace_no = alloc_no;
                discard = false;
            }
            break;
        case _HOOK_REALLOC: // TODO Design realloc test
            char buf[256];
            sprintf_s(buf, "FREE request_=%ld\n", lRequest);
            OutputDebugStringA(buf);
            break;
        case _HOOK_FREE:
            break;
        default:
            break;
        }
    }

    int result = TRUE;
    if (stored_alloc_hook)
    {   // Forward call to previously installed hook
        result = stored_alloc_hook(nAllocType, pvData, 
            nSize, nBlockUse, lRequest, szFileName, nLine);
    }
    return result;
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::SetAllocHook()
{
    assert(alloc_hook_set_ == false);
    discard = true;
    stored_alloc_hook = _CrtSetAllocHook(AllocHook);
    alloc_hook_set_ = true;
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::RevertAllocHook()
{
    assert(alloc_hook_set_ == true);
    // "warning C5039: '_CrtSetAllocHook', false positive
#pragma warning( push )            
#pragma warning( disable : 5039 ) 
    (void)_CrtSetAllocHook(stored_alloc_hook); 
    alloc_hook_set_ = false;
#pragma warning( pop )
    discard = false;
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestStart(
    std::function<std::string()> descriptor)
{
    // Hook directly so we can count number of allocations from this function
    // as well since it will offset recorded allocation request indices.
    SetAllocHook();

    // Create a memory checkpoint to diff with later to find leaks
    _CrtMemCheckpoint(&pre_state_);

    // Find leaking allocation from database built during previous test run
    // Note that this search indirectly allocates which makes it possible to
    // obtain pre-allocation no used for relative allocation request no calc
    break_alloc = no_break_alloc;
    const auto kvp_it = db_.find(descriptor());
    if (kvp_it != db_.end())
        break_alloc = kvp_it->second;

    // Initialize global pointers
    pointers = 
    {
        buffer,
        buffer + GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH,
        filename,
        &line
    };
    pointers.buffer_ptr[0] = 0;
    pointers.filename_ptr[0] = 0;

    pre_alloc_no = alloc_no;
    discard = 0;
    discard = false;

    // Determine allocation no based on relative information
    if (break_alloc != no_break_alloc)
        break_alloc += alloc_no; 

    printf("PRE ALLOC NO: %ld\n", pre_alloc_no);
}

long gtest_memleak_detector::MemoryLeakDetectorListener::Impl::ParseMemLeakAllocNo(_CrtMemState& mem_diff) const noexcept
{
    // Make scoped RPT hook since we only need to capture statistics
    static long parsed_alloc_no = no_break_alloc;
    //static auto stored_report_hook = _CrtGetReportHook();
    static const auto report_cb =
        [](int reportType, char* message, int* returnValue) noexcept -> int
    {
        UNREFERENCED_PARAMETER(reportType);
        UNREFERENCED_PARAMETER(returnValue);

        // IMPORTANT: Remember that this function must have noexcept/nothrow semantics
        //            since indirectly called by C-run-time.
        if (parsed_alloc_no == no_break_alloc)
        {
            // if not previously parsed on earlier invocation
            if (parsed_alloc_no == no_break_alloc && message)
                TryParseAllocNo(parsed_alloc_no, message); // TODO can be void
            //fprintf(stderr, message); // avoid std::cerr since CRT callback
        }
        if (returnValue)
            return *returnValue;
        return TRUE;
    };

    // "warning C5039: '_CrtSetAllocHook', false positive
#pragma warning( push )            
#pragma warning( disable : 5039 ) 
    // Replace pre-installed hook with lambda and dump statistics
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, report_cb);
#pragma warning( pop )
    _CrtMemDumpAllObjectsSince(&pre_state_);
    _CrtMemDumpStatistics(&mem_diff);
    // "warning C5039: '_CrtSetAllocHook', false positive
#pragma warning( push )            
#pragma warning( disable : 5039 ) 
    _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, report_cb);
#pragma warning( pop )
//    // Restore previous (pre-installed) hook
//    if (stored_report_hook)
//    {
//#pragma warning( push )            
//#pragma warning( disable : 5039 ) // Same motivation as above
//        _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, stored_report_hook); // TODO Incorrect?!
////        _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, stored_report_hook);
//#pragma warning( pop )
//    }

    return parsed_alloc_no;
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestEnd(
	std::function<std::string()> descriptor, bool passed)
{
    post_alloc_no = alloc_no;

    // Stack trace allocations do not matter since memory leak allocation is that exact allocation request
    // Anything being allocated after that point only adds to offset of next test

    printf("POST ALLOC NO: %ld\n", post_alloc_no);
    printf("DIFF ALLOC NO: %ld\n", (post_alloc_no - pre_alloc_no)); 
    printf("PRE TRACE: %ld\n", pre_trace_no);
    printf("POST TRACE: %ld\n", post_trace_no);
    printf("TRACE: %ld\n", (post_trace_no - pre_trace_no));

    pointers = trace_pointers();

    if (alloc_hook_set_)
        RevertAllocHook();

	// Avoid adding extra asserts if test is not passing anyway and the failing
    // logic is the main failure.
	// Another reason to do this is that gtest report false positives when 
    // reporting assertion failures due to memory allocation within gtest 
    // framework (bad).

    auto leak_alloc_no = no_break_alloc;
    auto leak_detected = false;
    if (passed)
    {
        _CrtMemState post_state;
        _CrtMemCheckpoint(&post_state);

        _CrtMemState mem_diff;
        leak_detected = _CrtMemDifference(&mem_diff, &pre_state_, &post_state) != 0;
        if (leak_detected)
            leak_alloc_no = ParseMemLeakAllocNo(mem_diff);
    }

    // Compute allocation number for test and store
    const auto stored_alloc_no = (leak_alloc_no != no_break_alloc) ?
        (leak_alloc_no - pre_alloc_no) :
        no_break_alloc;
    printf("PARSED ALLOC NO: %ld\n", leak_alloc_no);
    printf("STORED_ALLOC_NO: %ld\n", stored_alloc_no);
    
    // Fail test only if not failed due to assertion and a leak
    // has been detected.
    const auto description = descriptor();
    if (passed && leak_detected)
    {
        //auto description = DescribeTest(test_info);
        db_.insert_or_assign(description, stored_alloc_no);
        rerun_filter_.emplace_back(description);
        FailCurrentTest(leak_alloc_no, filename, line, buffer);
    }
    else
    {
        db_.insert_or_assign(description, stored_alloc_no);
    }
}

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE