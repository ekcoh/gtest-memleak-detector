// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#include "memory_leak_detector.h"

//#ifdef GTEST_MEMLEAK_DETECTOR_CRTDBG_AVAILABLE

#include <string>   // std::string
#include <exception>

#ifdef GTEST_MEMLEAK_DETECTOR_DEBUG
#define GTEST_MEMLEAK_DETECTOR_DBGLOG(fmt, ...) \
    gtest_memleak_detector::MemoryLeakDetector::Instance()->Log(fmt, __VA_ARGS__)
#else
#define GTEST_MEMLEAK_DETECTOR_DBGLOG(fmt, ...)
#endif // GTEST_MEMLEAK_DETECTOR_DEBUG

///////////////////////////////////////////////////////////////////////////////
// _CrtMemBlockHeader
//
// This normally opaque struct is vaguely documented here:
// https://docs.microsoft.com/en-us/visualstudio/debugger/crt-debug-heap-details?view=vs-2019
//
// Note that we are only interested in request counter so other fields are only
// relevant for getting padding right.
///////////////////////////////////////////////////////////////////////////////

#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE

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
extern "C" 
{ 
    static _CRT_ALLOC_HOOK stored_alloc_hook = NULL; 
}

static long           alloc_no = 0;
//long                  parsed_alloc_no;

extern "C" int GTestMemoryLeakDetector4ll0c470rh00k(
    int nAllocType, void* pvData,
    size_t nSize, int nBlockUse, long lRequest,
    const unsigned char* szFileName, int nLine) noexcept
{
    gtest_memleak_detector::MemoryLeakDetector::Instance()->OnAllocation(
        nAllocType, lRequest);

    int result = TRUE;
    if (stored_alloc_hook)
    {   // Forward call to previously installed hook
        result = stored_alloc_hook(nAllocType, pvData,
            nSize, nBlockUse, lRequest, szFileName, nLine);
    }
    return result;
}

bool try_parse_alloc_no(long& dst, const char* str) noexcept // TODO Make static class function
{
    // IMPORTANT: This function must have noexcept/nothrow semantics
    //            since indirectly called by C-run-time.
    if (!str)
        return false; // nullptr

    const auto first = strchr(str, '{');
    if (nullptr == first)
        return false; // failed (format error)

    const auto last = strchr(first, '}');
    if (nullptr == last)
        return false; // failed (format error)

    const auto len = last - first - 1;
    if (len <= 0 || len > 10) // max 10 digits
        return false; // failed (range error)

    // Parse allocation no as 32-bit integer, i.e.
    // max 10 digits + termination char
    char buffer[11]{ 0 };
    strncpy_s(buffer, first + 1, static_cast<size_t>(len));
    const auto value = strtol(buffer, nullptr, 10);
    if (value == LONG_MIN || value == LONG_MAX)
        return false; // failed

    dst = value;
    return true; // success
}

void gtest_memleak_detector::MemoryLeakDetector::OnReport(const char* message) noexcept
{
    long parsed_value;
    if (try_parse_alloc_no(parsed_value, message))
    {
        // Only parse leak originating from code exercised within test-body
        // we do not want leak reports on e.g. Google Test framework
        if (parsed_value > state_.pre_alloc_no && 
            parsed_value <= state_.post_alloc_no)
        {
            // Parse if within range and preceeding previously parsed leak
            // or if this is the first parsed leak for the current test case
            if (parsed_value < state_.parsed_alloc_no || 
                state_.parsed_alloc_no == no_break_alloc)
            {
                state_.parsed_alloc_no = parsed_value;
            }
        }
    }
}

extern "C" int report_callback(int reportType, char* message, int* returnValue)
{
    // IMPORTANT: Remember that this function must have noexcept/nothrow semantics
    //            since indirectly called by C-run-time.
    if (reportType == _CRT_WARN)
        gtest_memleak_detector::MemoryLeakDetector::Instance()->OnReport(message);

    if (returnValue)
        return *returnValue; // TODO Check what's expected
    return TRUE;
}

#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE

///////////////////////////////////////////////////////////////////////////////
// gtest_memleak_detector definitions
///////////////////////////////////////////////////////////////////////////////

class GTestFlagSaverAllocationMitigator : public ::testing::Test
{
    void TestBody() override { }
};

gtest_memleak_detector::MemoryLeakDetector*
    gtest_memleak_detector::MemoryLeakDetector::instance_ = nullptr;

gtest_memleak_detector::MemoryLeakDetector::MemoryLeakDetector(
    int argc, char** argv) 
    : pre_state_{ 0 }
    , stored_debug_flags_(0)
    , alloc_hook_set_(false)
    , fail_(nullptr)
#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    , stack_trace_()
#endif
{
    // Require binary path as first argument
    if (argc == 0)
        throw std::exception("at least executable name required");
    if (argv == nullptr)
        throw std::exception("missing command line arguments");

#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    // Get test binary file info from data-base
    if (_stat(argv[0], &file_info_) == 0)
    {
        file_path_ = MakeDatabaseFilePath(argv[0]); // TODO Use individual leak files instead
        if (!TryReadDatabase())
            std::remove(file_path_.c_str());
    }
#endif

    // Temporarily allocate a test case to mitigate differences in allocation
    // patterns caused by how the test is executed. Current implementation of
    // ::testing::internal::GTestFlagSaver causes problems since it will 
    // allocate at different points in the test flow.
    auto temp = std::make_unique<GTestFlagSaverAllocationMitigator>();
    temp.reset();

    //const auto result = _mkdir("MemoryLeaks");
    //if (result < 0)
    //    throw std::exception("Failed to create directory 'MemoryLeaks'");
    
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
    UNREFERENCED_PARAMETER(leak_file);
    UNREFERENCED_PARAMETER(leak_line);

    std::stringstream ss;
    ss << "Memory leak detected";
    if (leak_alloc_no >= 0)
        ss << " (Request: " << leak_alloc_no << ")";
    if (leak_trace && leak_trace[0] != 0)
        ss << " at:\n" << leak_trace;
    else
        ss << ". Re-run test to obtain stack-trace of the allocation causing the memory leak.";
    return ss.str();
}

std::string gtest_memleak_detector::MemoryLeakDetector::MakeDatabaseFilePath(
    const char* binary_file_path)
{
    if (!binary_file_path)
        throw std::exception();
    std::string path = binary_file_path;
    path += ".gt.memleaks";
    return path;
}

bool gtest_memleak_detector::MemoryLeakDetector::ReadDatabase()
{
#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    // Open file
    std::ifstream in;
    in.open(file_path_);
    if (!in)
    {
        GTEST_MEMLEAK_DETECTOR_DBGLOG("Database: %s", "File do not exist");
        return true; // file do not exist
    }

    // Parse file size
    decltype(file_info_.st_size) st_size = 0;
    in >> st_size;
    if (st_size != file_info_.st_size)
    {
        GTEST_MEMLEAK_DETECTOR_DBGLOG("Database: %s", "Different binary file size");
        return false; // different file size
    }

    // Parse last modification timestamp
    decltype(file_info_.st_mtime) st_mtime = 0;
    in >> st_mtime;
    if (st_mtime != file_info_.st_mtime)
    {
        GTEST_MEMLEAK_DETECTOR_DBGLOG("Database: %s", "Different modification data");
        return false; // different modification data
    }

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

    GTEST_MEMLEAK_DETECTOR_DBGLOG("Database: %s", "Success");
    return true; // success
#else
    return false; // disabled
#endif
}

bool gtest_memleak_detector::MemoryLeakDetector::TryReadDatabase()
{
#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    try
    {
        instance_ = this;
        ReadDatabase();
        instance_ = nullptr;
    }
    catch (std::bad_alloc&)
    {
        instance_ = nullptr;
        throw; // re-throw
    }
    catch (...)
    {
        instance_ = nullptr;
        return false; // corrupt or incompatible file
    }
    
    return true; // successfully parsed all data
#else
    return true; // indicate success but leave db empty
#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
}

void gtest_memleak_detector::MemoryLeakDetector::WriteDatabase()
{
#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    std::ofstream out;
    out.open(file_path_);
    out << file_info_.st_size << '\n'
        << file_info_.st_mtime << '\n'
        << db_.size() << '\n';
    for (auto& kvp : db_)
        out << kvp.first << '\n' << kvp.second << '\n';
    out.flush();
    out.close();
#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
}

//void gtest_memleak_detector::MemoryLeakDetector::WriteLeakFile(long leak_alloc_no)
//{
//    std::ofstream out;
//    out.open("MemoryLeaks/" + hash_)
//}

#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE

void gtest_memleak_detector::MemoryLeakDetector::CaptureLeakStackTrace()
{
    try 
    {
        state_.discard = true;
        state_.pre_trace_no = alloc_no;
        stack_trace_.Reset(); // TODO Guard against different thread
        stack_trace_.ShowCallstack();
        state_.post_trace_no = alloc_no;
        state_.discard = false;

        switch (stack_trace_.CurrentState())
        {
        case StackTrace::State::Completed:
            SetTrace(stack_trace_.GetLocation(), stack_trace_.Stream().str());
            break;
        case StackTrace::State::Capture:
        case StackTrace::State::Scanning:
        case StackTrace::State::Exception:
        default:
            break;
        }
    }
    catch (...)
    {
        // Ignore
    }
}

#ifdef GTEST_MEMLEAK_DETECTOR_DEBUG

void gtest_memleak_detector::MemoryLeakDetector::LogStackTrace()
{
    state_.discard = true;
    try 
    {
        stack_trace_.Reset();
        stack_trace_.ShowCallstack();
        Log("%s", stack_trace_.Stream().str().c_str());
    }
    catch (...)
    {
        ResetDebugBuffer();
    }
    state_.discard = false;
}

#endif GTEST_MEMLEAK_DETECTOR_DEBUG

void gtest_memleak_detector::MemoryLeakDetector::OnAllocation(
    int nAllocType, long lRequest)
{
    switch (nAllocType)
    {
    case _HOOK_ALLOC:
    case _HOOK_REALLOC:
        alloc_no = lRequest;
        if (state_.discard)
            break;
        GTEST_MEMLEAK_DETECTOR_DBGLOG("# alloc_no: %ld, relative_no: %ld\n", 
            alloc_no, alloc_no - gtest_memleak_detector::MemoryLeakDetector::state_.pre_alloc_no);
#ifdef GTEST_MEMLEAK_DETECTOR_DEBUG
        LogStackTrace();
#endif
        if (lRequest == state_.break_alloc)
            CaptureLeakStackTrace();
        break;
    case _HOOK_FREE: // fall-through
    default:
        break;
    }
}

#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE

void gtest_memleak_detector::MemoryLeakDetector::SetAllocHook()
{
#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    assert(alloc_hook_set_ == false);
    stored_alloc_hook = _CrtSetAllocHook(GTestMemoryLeakDetector4ll0c470rh00k);
    alloc_hook_set_ = true;
#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
}

void gtest_memleak_detector::MemoryLeakDetector::RevertAllocHook()
{
#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    assert(alloc_hook_set_ == true);
    // "warning C5039: '_CrtSetAllocHook', false positive
    // Currently no known approach to avoid this.
    // stored_alloc_hook should not throw since C API
    #pragma warning( push )            
    #pragma warning( disable : 5039 ) 
    (void)_CrtSetAllocHook(stored_alloc_hook); 
    #pragma warning( pop )
    alloc_hook_set_ = false;
#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
}

void gtest_memleak_detector::MemoryLeakDetector::SetTrace(
    const Location& location, std::string stack_trace)
{
    location_ = location;
    trace_ = std::move(stack_trace);
}

void gtest_memleak_detector::MemoryLeakDetector::Start(
    std::function<std::string()> descriptor)
{
#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    ::testing::UnitTest::GetInstance();
    if (instance_ != nullptr)
        throw std::exception("Parallel execution not supported\n");
    instance_ = this;

    GTEST_MEMLEAK_DETECTOR_DBGLOG("%s", "begin-first ----------\n");

    state_ = State(); // reset

    GTEST_MEMLEAK_DETECTOR_DBGLOG("Process ID: %lu\n", GetProcessId(GetCurrentProcess()));
    GTEST_MEMLEAK_DETECTOR_DBGLOG("Thread ID:  %lu\n", GetThreadId(GetCurrentThread()));
    GTEST_MEMLEAK_DETECTOR_DBGLOG("Database:   %s\n", file_path_.c_str());

    // Hook directly so we can count number of allocations from this function
    // as well since it will offset recorded allocation request indices.
    SetAllocHook();

    // Find leaking allocation from database built during previous test run
    // Note that this search indirectly allocates which makes it possible to
    // obtain pre-allocation no used for relative allocation request no calc
    {
        auto description_ = descriptor();
        hash_ = std::hash<std::string>()(description_);
        const auto kvp_it = db_.find(description_);
        if (kvp_it != db_.end())
            state_.break_alloc = kvp_it->second;
    }
    
    // Determine allocation no based on relative information
    state_.pre_alloc_no = alloc_no;
    if (state_.break_alloc != no_break_alloc)
        state_.break_alloc += alloc_no;

    // Create a memory checkpoint to diff with later to find leaks
    // NOTE: Allocations below will be excluded
    _CrtMemCheckpoint(&pre_state_);

    //GTEST_MEMLEAK_DETECTOR_DBGLOG("PRE ALLOC NO: %ld, BREAK ALLOC NO: %ld, PRE-REQ: %ld\n", state_.pre_alloc_no, state_.break_alloc, pre_state_.pBlockHeader->lRequest);

#else
    UNREFERENCED_PARAMETER(descriptor);
#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE

    GTEST_MEMLEAK_DETECTOR_DBGLOG("%s", "end-first ----------\n");
}

void gtest_memleak_detector::MemoryLeakDetector::End(
	std::function<std::string()> descriptor, bool passed)
{
#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
    GTEST_MEMLEAK_DETECTOR_DBGLOG("%s", "begin-end ----------\n");

    assert(instance_ != nullptr);

    state_.post_alloc_no = alloc_no;

    // Unhook to avoid further allocation callbacks from code below
    if (alloc_hook_set_)
        RevertAllocHook();

	// Avoid adding extra asserts if test is not passing anyway and the failing
    // logic is the main failure.
	// Another reason to do this is that gtest report false positives when 
    // reporting assertion failures due to memory allocation within gtest 
    // framework (bad).
    auto leak_alloc_no = no_break_alloc;
    auto leak_detected = false;
    if (passed) // Avoid reporting leaks if previous assertion failure
    {
        _CrtMemState post_state;
        _CrtMemCheckpoint(&post_state);

        _CrtMemState mem_diff;
        leak_detected = _CrtMemDifference(&mem_diff, &pre_state_, &post_state) != 0;
        if (leak_detected)
        {
            //parsed_alloc_no = no_break_alloc; // init
            if (_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, report_callback) == -1)
                throw std::exception("Failed to install CRT report hook");
            _CrtMemDumpAllObjectsSince(&pre_state_);
            //_CrtMemDumpStatistics(&mem_diff);
            if (_CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, report_callback) == -1)
                throw std::exception("Failed to remove CRT report hook");
            leak_alloc_no = state_.parsed_alloc_no;
//            assert(leak_alloc_no > state_.pre_alloc_no);
        }
    }

    // Compute allocation number for test and store
    const auto relative_leak_alloc_no = (leak_alloc_no != no_break_alloc) ?
        (leak_alloc_no - state_.pre_alloc_no) :
        no_break_alloc;

    // Stack trace allocations do not matter since memory leak allocation is that exact allocation request
    // Anything being allocated after that point only adds to offset of next test

    GTEST_MEMLEAK_DETECTOR_DBGLOG("- pre_alloc_no:      %ld\n", state_.pre_alloc_no);
    GTEST_MEMLEAK_DETECTOR_DBGLOG("- pre_trace_no:      %ld\n", state_.pre_trace_no);
    GTEST_MEMLEAK_DETECTOR_DBGLOG("- post_trace_no:     %ld\n", state_.post_trace_no);
    GTEST_MEMLEAK_DETECTOR_DBGLOG("- post_alloc_no:     %ld\n", state_.post_alloc_no);
    GTEST_MEMLEAK_DETECTOR_DBGLOG("- diff_allocs:       %ld\n", (state_.post_alloc_no - state_.pre_alloc_no));
    GTEST_MEMLEAK_DETECTOR_DBGLOG("- trace_allocs:      %ld\n", (state_.post_trace_no - state_.pre_trace_no));
    GTEST_MEMLEAK_DETECTOR_DBGLOG("- abs_leak_alloc_no: %ld\n", leak_alloc_no);
    GTEST_MEMLEAK_DETECTOR_DBGLOG("- leak_alloc_no:     %ld\n", relative_leak_alloc_no);

#ifdef GTEST_MEMLEAK_DETECTOR_DEBUG
    DumpAndResetLog();
#endif

    // Fail test only if not failed due to assertion and a leak
    // has been detected.
    if (passed && leak_detected) // TODO Assert deterministic allocations, otherwise warn
    {
        auto description = descriptor();
        db_.insert_or_assign(description, relative_leak_alloc_no);
        rerun_filter_.emplace_back(description);
        if (fail_)
            fail_(leak_alloc_no, location_.file.c_str(), location_.line, trace_.c_str());
    }
    else
    {
        db_.insert_or_assign(descriptor(), relative_leak_alloc_no);
    }

    instance_ = nullptr; // TODO Scoped
#else
    UNREFERENCED_PARAMETER(descriptor);
    UNREFERENCED_PARAMETER(passed);
#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
}

//#endif // GTEST_MEMLEAK_DETECTOR_CRTDBG_AVAILABLE