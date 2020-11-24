// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#include "memory_leak_detector_listener_impl.h"

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE

#include <string>   // std::string
#include <exception>

// Constants
constexpr long no_break_alloc = -1;

// Locals
extern "C"
{
    _CRT_ALLOC_HOOK stored_alloc_hook = NULL;
}

trace_pointers  pointers;
//long            parsed_alloc_no = no_break_alloc;
long            break_alloc = no_break_alloc;
bool            discard = false;

long            alloc_no = 0;

long            pre_alloc_no = 0;
long            post_alloc_no = 0;
long            pre_trace_no = 0;
long            post_trace_no = 0;

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

// TODO Convert to extern C
extern "C" int AllocHook(
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
                gtest_memleak_detector::StackTrace trace(pointers.buffer_ptr,
                    pointers.buffer_ptr + GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH,
                    pointers.filename_ptr, pointers.line_ptr, __FILE__);
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
    assert(pointers.buffer_ptr == nullptr);

    // TEMP
    pre_alloc_no = 0;
    post_alloc_no = 0;
    pre_trace_no = 0;
    post_trace_no = 0;

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

bool try_parse_alloc_no(long& dst, const char* str) noexcept
{
    // IMPORTANT: This function must have noexcept/nothrow semantics
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
    if (len <= 0 || len > 10) // max 10 digits
        return false; // failed (range error)

    // Parse allocation no as 32-bit integer, i.e.
    // max 10 digits + termination char
    char buffer[11];
    strncpy_s(buffer, start + 1, static_cast<size_t>(len));
    const auto allocation_no = strtol(buffer, nullptr, 10);
    if (allocation_no == LONG_MIN || allocation_no == LONG_MAX)
        return false; // failed

    dst = allocation_no;
    return true; // success
}

long parsed_alloc_no;

extern "C" int report_callback(int reportType, char* message, int* returnValue)
{
    UNREFERENCED_PARAMETER(reportType);
    UNREFERENCED_PARAMETER(returnValue);

    if (reportType == _CRT_WARN)
    {
        // IMPORTANT: Remember that this function must have noexcept/nothrow semantics
        //            since indirectly called by C-run-time.
        if (parsed_alloc_no == no_break_alloc)
        {
            // if not previously parsed on earlier invocation
            if (parsed_alloc_no == no_break_alloc && message)
                try_parse_alloc_no(parsed_alloc_no, message); // TODO can be void
            fprintf(stderr, message); // avoid std::cerr since CRT callback
        }
    }
    
    if (returnValue)
        return *returnValue; // TODO Check wahts expected
    return TRUE;
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
        {
            parsed_alloc_no = no_break_alloc; // init
            if (_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, report_callback) == -1)
                throw std::exception("Failed to install CRT report hook");
            _CrtMemDumpAllObjectsSince(&pre_state_);
            _CrtMemDumpStatistics(&mem_diff);
            if (_CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, report_callback) == -1)
                throw std::exception("Failed to remove CRT report hook");
            leak_alloc_no = parsed_alloc_no;
            assert(leak_alloc_no > pre_alloc_no);
        }
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