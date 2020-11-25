// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#include "memory_leak_detector.h"

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE

#include <string>   // std::string
#include <exception>

#define GTEST_MEMLEAK_DETECTOR_DEBUG_INTERNALS

///////////////////////////////////////////////////////////////////////////////
// test_case_data
///////////////////////////////////////////////////////////////////////////////

static constexpr long no_break_alloc = -1;

struct test_case_data
{
    gtest_memleak_detector::Buffer*   buffer_ptr = nullptr;
    gtest_memleak_detector::Location* location_ptr = nullptr;
    long pre_alloc_no  = 0;
    long post_alloc_no = 0;
    long pre_trace_no  = 0;
    long post_trace_no = 0;
    long break_alloc   = no_break_alloc;
    bool discard       = false;
};

///////////////////////////////////////////////////////////////////////////////
// _CrtMemBlockHeader
//
// This normally opaque struct is vaguely documented here:
// https://docs.microsoft.com/en-us/visualstudio/debugger/crt-debug-heap-details?view=vs-2019
//
// Note that we are only interested in request counter so other fields are only
// relevant for getting padding right.
///////////////////////////////////////////////////////////////////////////////

extern "C" {

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

} // extern "C"

// Constants and locals
extern "C" { static _CRT_ALLOC_HOOK stored_alloc_hook = NULL; }
static test_case_data state;
static long           alloc_no = 0;

///////////////////////////////////////////////////////////////////////////////
// gtest_memleak_detector definitions
///////////////////////////////////////////////////////////////////////////////

const char* gtest_memleak_detector::MemoryLeakDetector::database_file_suffix
    = "gt.memleaks";

bool gtest_memleak_detector::MemoryLeakDetector::TryParseAllocNo(
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

gtest_memleak_detector::MemoryLeakDetector::MemoryLeakDetector(
    int argc, char** argv) 
    : pre_state_{ 0 }
    , stored_debug_flags_(0)
    , alloc_hook_set_(false)
    , fail_(nullptr)
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
	stored_debug_flags_ = _CrtSetDbgFlag(
        _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); 
}

void gtest_memleak_detector::MemoryLeakDetector::SetFailureCallback(FailureCallback cb)
{
    fail_ = cb;
}

std::string gtest_memleak_detector::MemoryLeakDetector::MakeFailureMessage(
    long leak_alloc_no,
    const char* leak_file,
    unsigned long leak_line,
    const char* leak_trace)
{
    std::stringstream ss;
    ss << "Memory leak detected.";
    if (leak_alloc_no >= 0)
        ss << "\n- Allocation request no: " << leak_alloc_no;
    if (leak_file != nullptr && leak_file[0] != 0)
    {
        ss << "\n- Origin: " << leak_file;
        if (leak_line != static_cast<unsigned long>(-1))
            ss << ": " << leak_line;
    }
    if (leak_trace && leak_trace[0] != 0)
    {
        ss << "\n\nStacktrace:\n" << leak_trace;
    }
    else
    {
        ss << "\n\n(Re-run the test again to obtain stack-trace for the "
            "allocation causing the memory-leak.)";
    }
    return ss.str();
}

std::string gtest_memleak_detector::MemoryLeakDetector::MakeDatabaseFilePath(
    const char* binary_file_path)
{
    if (!binary_file_path)
        throw std::exception();
    std::string path = binary_file_path;
    path += '.';
    path += database_file_suffix;
    return path;
}

bool gtest_memleak_detector::MemoryLeakDetector::ReadDatabase()
{
    // Open file
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

bool gtest_memleak_detector::MemoryLeakDetector::TryReadDatabase()
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

void gtest_memleak_detector::MemoryLeakDetector::WriteDatabase()
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
    switch (nAllocType)
    {
    case _HOOK_ALLOC:
    case _HOOK_REALLOC:
        if (state.discard)
            break;
        alloc_no = lRequest;
        if (lRequest == state.break_alloc)
        {
            state.discard = true;
            state.pre_trace_no = alloc_no;
            gtest_memleak_detector::StackTrace trace(
                state.buffer_ptr, state.location_ptr, __FILE__);
            trace.ShowCallstack(); // Result stored in buffer_ptr
            state.post_trace_no = alloc_no;
            state.discard = false;
        }
        break;
    case _HOOK_FREE: // fall-through
    default:
        break;
    }

    int result = TRUE;
    if (stored_alloc_hook)
    {   // Forward call to previously installed hook
        result = stored_alloc_hook(nAllocType, pvData, 
            nSize, nBlockUse, lRequest, szFileName, nLine);
    }
    return result;
}

void gtest_memleak_detector::MemoryLeakDetector::SetAllocHook()
{
    assert(alloc_hook_set_ == false);
    stored_alloc_hook = _CrtSetAllocHook(AllocHook);
    alloc_hook_set_ = true;
}

void gtest_memleak_detector::MemoryLeakDetector::RevertAllocHook()
{
    assert(alloc_hook_set_ == true);
    // "warning C5039: '_CrtSetAllocHook', false positive
    // Currently no known approach to avoid this.
    // stored_alloc_hook should not throw since C API
    #pragma warning( push )            
    #pragma warning( disable : 5039 ) 
    (void)_CrtSetAllocHook(stored_alloc_hook); 
    #pragma warning( pop )
    alloc_hook_set_ = false;
}

void gtest_memleak_detector::MemoryLeakDetector::Start(
    std::function<std::string()> descriptor)
{
    state = test_case_data();
    state.buffer_ptr = &buffer;
    state.location_ptr = &location;

    // Hook directly so we can count number of allocations from this function
    // as well since it will offset recorded allocation request indices.
    SetAllocHook();

    //// Create a memory checkpoint to diff with later to find leaks
    //// NOTE: Allocations below will be excluded
    //_CrtMemCheckpoint(&pre_state_);

    // Find leaking allocation from database built during previous test run
    // Note that this search indirectly allocates which makes it possible to
    // obtain pre-allocation no used for relative allocation request no calc
    const auto kvp_it = db_.find(descriptor());
    if (kvp_it != db_.end())
        state.break_alloc = kvp_it->second;
    
    // Reset internals
    memset(buffer.data, 0, sizeof(sizeof(buffer) / sizeof(char)));
    buffer.last = buffer.data;
    buffer.end = buffer.data + GTEST_MEMLEAK_DETECTOR_STACKTRACE_MAX_LENGTH;
    location.file[0] = 0;
    location.line = Location::invalid_line;

    //pointers.buffer_ptr[0] = 0;
    //pointers.filename_ptr[0] = 0;

    state.pre_alloc_no = alloc_no;

    // Determine allocation no based on relative information
    if (state.break_alloc != no_break_alloc)
        state.break_alloc += alloc_no; 

    printf("PRE ALLOC NO: %ld\n", state.pre_alloc_no);

    // Create a memory checkpoint to diff with later to find leaks
    // NOTE: Allocations below will be excluded
    _CrtMemCheckpoint(&pre_state_);
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
        long parsed_value;
        if (try_parse_alloc_no(parsed_value, message))
        {
            if (parsed_value < parsed_alloc_no ||  // if leak happening earlier in flow
                parsed_alloc_no == no_break_alloc) // if leak not yet parsed
            {
                parsed_alloc_no = parsed_value;
            }
        }
    }
    
    if (returnValue)
        return *returnValue; // TODO Check what's expected
    return TRUE;
}

void gtest_memleak_detector::MemoryLeakDetector::End(
	std::function<std::string()> descriptor, bool passed)
{
    state.post_alloc_no = alloc_no;

    // Stack trace allocations do not matter since memory leak allocation is that exact allocation request
    // Anything being allocated after that point only adds to offset of next test

    printf("POST ALLOC NO: %ld\n", state.post_alloc_no);
    printf("DIFF ALLOC NO: %ld\n", (state.post_alloc_no - state.pre_alloc_no));
    printf("PRE TRACE: %ld\n", state.pre_trace_no);
    printf("POST TRACE: %ld\n", state.post_trace_no);
    printf("TRACE: %ld\n", (state.post_trace_no - state.pre_trace_no));

    //buffer_ptr = nullptr;
    //location_ptr = nullptr;

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
            assert(leak_alloc_no > state.pre_alloc_no);
        }
    }

    // Compute allocation number for test and store
    const auto stored_alloc_no = (leak_alloc_no != no_break_alloc) ?
        (leak_alloc_no - state.pre_alloc_no) :
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
        if (fail_)
            fail_(leak_alloc_no, location.file, location.line, buffer.data);
    }
    else
    {
        db_.insert_or_assign(description, stored_alloc_no);
    }
}

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE