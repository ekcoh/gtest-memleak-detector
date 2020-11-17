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

#ifndef GTEST_MEMLEAK_DETECTOR_FILENAME
#define GTEST_MEMLEAK_DETECTOR_FILENAME "gtest_memleak_detector.allocation"
#endif // GTEST_MEMLEAK_DETECTOR_FILENAME

#ifndef GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME
#define GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME GTEST_MEMLEAK_DETECTOR_FILENAME ## ".tmp"
#endif // GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE

// Constants
const long gtest_memleak_detector::MemoryLeakDetectorListener::Impl::no_break_alloc = -1;

// Locals
long gtest_memleak_detector::MemoryLeakDetectorListener::Impl::parsed_alloc_no = -1;

_CRT_ALLOC_HOOK gtest_memleak_detector::MemoryLeakDetectorListener::Impl::stored_alloc_hook = nullptr;
char* gtest_memleak_detector::MemoryLeakDetectorListener::Impl::buffer_ptr = nullptr;
char* gtest_memleak_detector::MemoryLeakDetectorListener::Impl::filename_ptr = nullptr;
unsigned long* gtest_memleak_detector::MemoryLeakDetectorListener::Impl::line_ptr = nullptr;

long break_alloc = -1;

// Translation unit utility functions
namespace {

unsigned long discarded = 0;
bool discard = false;

///////////////////////////////////////////////////////////////////////////////
// Trace declaration - Stack trace formatting
///////////////////////////////////////////////////////////////////////////////

class Trace final : public StackWalker
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
    Trace(char* buffer, char* buffer_end, char* filename, unsigned long* line);

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

Trace::Trace(char* buffer, char* buffer_end, char* filename, unsigned long* line)
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

bool Trace::Filter(const CallstackEntry& entry) noexcept
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

void Trace::Format(CallstackEntry& entry)
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

void Trace::OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr)
{   // Discard dbg help error message, this is expected for e.g. malloc/realloc/free
    (void)(szFuncName);
    (void)(gle);
    (void)(addr);
}

void Trace::OnCallstackEntry(CallstackEntryType eType, CallstackEntry& entry) 
{
    if (eType == lastEntry || entry.offset == 0)
        return;

    switch (state)
    {
    case State::Pre:
        // First stack trace hit of relevance is alloc_hook
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

std::string describe_test(const ::testing::TestInfo& test_info)
{
    std::stringstream ss;
    ss << test_info.test_suite_name();
    ss << "::";
    ss << test_info.name();
    if (test_info.value_param() != nullptr)
    {
        ss << '/';
        ss << test_info.value_param();
    }
    if (test_info.type_param() != nullptr)
    {
        ss << '/';
        ss << test_info.type_param();
    }
    return ss.str();
} 

} // anonomuous namespace

///////////////////////////////////////////////////////////////////////////////
// gtest_memleak_detector definitions
///////////////////////////////////////////////////////////////////////////////

bool gtest_memleak_detector::MemoryLeakDetectorListener::Impl::try_parse_alloc_no(
    long& dst, const char* str) noexcept
{
    // IMPORTANT: This function must have noexcept/nothrow semantics
    //            since indirectly called by C-run-time.

    assert(str);

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

bool gtest_memleak_detector::MemoryLeakDetectorListener::Impl::read_and_compare(
    int argc, char** argv)
{
    assert(in_);

    using st_size_type = decltype(file_info_.st_size);
    using st_time_type = decltype(file_info_.st_mtime);

    st_size_type st_size = 0;
    st_time_type st_mtime = 0;

    in_ >> st_size;
    if (st_size != file_info_.st_size)
        return false; // different file size (TODO Remove?)

    in_ >> st_mtime; 
    if (st_mtime != file_info_.st_mtime)
        return false; // different modification data

    int prev_argc;
    in_ >> prev_argc;
    if (prev_argc != argc)
        return false; // different number of args

    std::string prev_arg;
    in_ >> std::ws; // discard white-space prior to getline
    for (auto i = 0; i < argc; ++i)
    {
        std::getline(in_, prev_arg);
        if (0 != strcmp(prev_arg.c_str(), argv[i]))
            return false; // different arg
    }

    return true; // successfully read and equal
}

gtest_memleak_detector::MemoryLeakDetectorListener::Impl::Impl(int argc, char** argv) :
	pre_state_{ 0 },
    alloc_hook_set_(false),
    break_alloc_(no_break_alloc)
{
	if (argc > 0)
	{
        // Get file info
		if (_stat(argv[0], &file_info_) != 0)
			memset(&file_info_, 0, sizeof(struct _stat));
	}

    // Write timestamps and arguments to file.
    // This is needed to determine if a consecutive test run has the same configuration
    out_.open(GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME);
    out_ << file_info_.st_size << '\n' 
         << file_info_.st_mtime << '\n';
    out_ << argc << '\n';
    for (auto i = 0; i < argc; ++i)
    {
        out_ << argv[i] << '\n';
    }

    //if (IsDebuggerPresent())
    //{
        in_.open(GTEST_MEMLEAK_DETECTOR_FILENAME);
        if (in_)
        {
            if (!read_and_compare(argc, argv)) // TODO Delete file if corrupt
                in_.close(); // failed, invalid or not applicable
        }
//    }

	// Turn on debug allocation
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); 
}

gtest_memleak_detector::MemoryLeakDetectorListener::Impl::~Impl()
{
    // Empty
}

int gtest_memleak_detector::MemoryLeakDetectorListener::Impl::alloc_hook(
    int nAllocType, void* pvData,
    size_t nSize, int nBlockUse, long lRequest,
    const unsigned char* szFileName, int nLine) noexcept
{
    UNREFERENCED_PARAMETER(pvData);
    UNREFERENCED_PARAMETER(nSize);
    UNREFERENCED_PARAMETER(nBlockUse);
    UNREFERENCED_PARAMETER(szFileName);
    UNREFERENCED_PARAMETER(nLine);

    if (discard)
    {   // allocation FREE do not drive request no
        if (nAllocType != _HOOK_FREE)
            ++discarded;
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
                //discarded = 0;
                {   // Scoped to allow computing discarded allocations
                    Trace trace(buffer_ptr,
                        buffer_ptr + GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH,
                        filename_ptr, line_ptr);
                    trace.ShowCallstack(); // Result stored in buffer_ptr
                }
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

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::set_alloc_hook()
{
    assert(alloc_hook_set_ == false);
    discard = true;
    stored_alloc_hook = _CrtSetAllocHook(alloc_hook);
    alloc_hook_set_ = true;
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::revert_alloc_hook()
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

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::set_globals()
{
    buffer_ptr = buffer;
    buffer_ptr[0] = 0;
    filename_ptr = filename;
    filename_ptr[0] = 0;
    line_ptr = &line;
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::unset_globals()
{
    buffer_ptr = nullptr;
    filename_ptr = nullptr;
    line_ptr = nullptr;
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestStart(
	const ::testing::TestInfo& test_info)
{
    // Hook directly so we can count number of allocations from this function
    // as well since it will offset recorded allocation request indices.
    set_alloc_hook();

    parsed_alloc_no = no_break_alloc;
	_CrtMemCheckpoint(&pre_state_);

    if (/*!IsDebuggerPresent() ||*/ !in_)
    {
        revert_alloc_hook();
        return; // no applicable results to use or debugger not attached
    }

    std::string test_description;
    in_ >> break_alloc_;
    in_ >> test_description;

    const auto description = describe_test(test_info);
    if (description != test_description)
    {
        in_.close(); // invalidate
        revert_alloc_hook();
        return;
    }

    break_alloc = break_alloc_;

    if (break_alloc_ != no_break_alloc)
    {   // Set allocation no to break on
         //_CrtSetBreakAlloc(break_alloc_); // TODO This is annoying when debugging
        break_alloc_ = no_break_alloc;
    }

    set_globals();

    discard = false;
    //break_alloc += discarded;
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestEnd(
	const ::testing::TestInfo& test_info)
{
    if (alloc_hook_set_)
    {
        revert_alloc_hook();
        buffer_ptr = nullptr;
    }       

	// Avoid adding extra asserts if test is not passing anyway and the failing
    // logic is the main failure.
	// Another reason to do this is that gtest report false positives when 
    // reporting assertion failures due to memory allocation within gtest 
    // framework (bad).

    auto leak_detected = false;
    if (test_info.result()->Passed())
    {
        _CrtMemState post_state;
        _CrtMemCheckpoint(&post_state);

        _CrtMemState mem_diff;
        leak_detected = _CrtMemDifference(&mem_diff, &pre_state_, &post_state) != 0;
        if (leak_detected)
        {
            // Dump objects and statistics via redirect hook
            // This indirectly sets parsed_alloc_no
            //std::cerr << "Failure: Memory leak detected.\n\n";
            
            // Make scoped RPT hook since we only need to capture statistics
            static auto stored_report_hook = _CrtGetReportHook();
            static const auto report_cb = 
                [](int reportType, char* message, int* returnValue) noexcept -> int 
            {
                // IMPORTANT: Remember that this function must have noexcept/nothrow semantics
                //            since indirectly called by C-run-time.
                auto result = TRUE;
                if (stored_report_hook)
                    result = stored_report_hook(reportType, message, returnValue);
                if (parsed_alloc_no != no_break_alloc)
                    return result; // already parsed on earlier invocation
                if (message)
                    try_parse_alloc_no(parsed_alloc_no, message);
                //fprintf(stderr, message); // avoid std::cerr since CRT callback
                return result;
            };

            // "warning C5039: '_CrtSetAllocHook', false positive
#pragma warning( push )            
#pragma warning( disable : 5039 ) 
            // Replace pre-installed hook with lambda and dump statistics
            _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, report_cb);
#pragma warning( pop )
            _CrtMemDumpAllObjectsSince(&pre_state_);
            _CrtMemDumpStatistics(&mem_diff);

            // Restore previous (preinstalled) hook
            if (stored_report_hook)
            {
#pragma warning( push )            
#pragma warning( disable : 5039 ) // Same motivation as above
                _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, report_cb); // TODO Incorrect?!
                _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, stored_report_hook);
#pragma warning( pop )
            }
        }
    }

	// Write test information to output file, note that this 
    // potentially allocates memory on its own for stream buffers.
	if (out_)
	{
        out_ << parsed_alloc_no << '\n';		
        out_ << describe_test(test_info) << '\n';
	}

    // Fail test only if not failed due to assertion and a leak
    // has been detected.
    if (test_info.result()->Passed() && leak_detected)
        fail();
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::fail()
{
    std::stringstream ss;
    ss << "Memory leak detected in scope of this test.";
    ss << "discarded: " << discarded << "\n";
    if (parsed_alloc_no != no_break_alloc)
        ss << "\n- Allocation request no: " << parsed_alloc_no;
    if (buffer[0] != '\0')
    {
        if (filename[0] != 0)
            ss << "\n- Origin: " << filename << ": " << line;
        ss << "\n\nStacktrace:\n" << buffer;
    }
    else
    {
        ss << "\n\n(Re-run the exact same test-suite (Repeat Last Run) to "
            "obtain stack-trace for the memory leak based on recorded "
            "allocations. Will require 2 re-runs to: "
            "1 - Record allocations, "
            "2 - Record with stack-trace, "
            "3 - Show result)";
    }

    GTEST_MESSAGE_AT_(filename, static_cast<int>(line), ss.str().c_str(),
        ::testing::TestPartResult::kNonFatalFailure);
   // GTEST_NONFATAL_FAILURE_("Memory leak detected");
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestProgramStart(
	const ::testing::UnitTest& test)
{
    UNREFERENCED_PARAMETER(test);
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestProgramEnd(
	const ::testing::UnitTest& test)
{
    UNREFERENCED_PARAMETER(test);

    in_.close();
	out_.close();

    // Clean-up temporary files
	std::remove(GTEST_MEMLEAK_DETECTOR_FILENAME);
	std::rename(GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME, 
                GTEST_MEMLEAK_DETECTOR_FILENAME);
}

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE