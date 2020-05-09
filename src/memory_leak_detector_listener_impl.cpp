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

// Translation unit utility functions
namespace {

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

bool gtest_memleak_detector::MemoryLeakDetectorListener::Impl::try_parse_alloc_no(
    long& dst, const char* str) noexcept
{
    // IMPORTANT: Remember that this function must have noexcept/nothrow semantics
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

    if (IsDebuggerPresent())
    {
        in_.open(GTEST_MEMLEAK_DETECTOR_FILENAME);
        if (in_)
        {
            if (!read_and_compare(argc, argv))
                in_.close(); // failed, invalid or not applicable
        }
    }

	// Turn on debug allocation
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); 
}

gtest_memleak_detector::MemoryLeakDetectorListener::Impl::~Impl()
{
	// Empty
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestStart(
	const ::testing::TestInfo& test_info)
{
    parsed_alloc_no = no_break_alloc;
	_CrtMemCheckpoint(&pre_state_);

    if (!IsDebuggerPresent() || !in_)
        return; // no applicable results to use or debugger not attached

    std::string test_description;
    in_ >> break_alloc_;
    in_ >> test_description;

    const auto description = describe_test(test_info);
    if (description != test_description)
    {
        in_.close(); // invalidate
        return;
    }
    
    if (break_alloc_ != no_break_alloc)
    {   // Set allocation no to break on
        _CrtSetBreakAlloc(break_alloc_);
        break_alloc_ = no_break_alloc;
    }
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestEnd(
	const ::testing::TestInfo& test_info)
{
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
            std::cerr << "Failure: Memory leak detected.\n\n";
            
            // Make scoped RPT hook since we only need to capture statistics
            static auto preinstalled_hook = _CrtGetReportHook();
            static const auto report_cb = [](int reportType, char* message, int* returnValue) noexcept -> int 
            {
                // IMPORTANT: Remember that this function must have noexcept/nothrow semantics
                //            since indirectly called by C-run-time.
                auto result = TRUE;
                if (preinstalled_hook)
                    result = preinstalled_hook(reportType, message, returnValue);
                if (parsed_alloc_no != no_break_alloc)
                    return result; // already parsed on earlier invocation
                if (message)
                    try_parse_alloc_no(parsed_alloc_no, message);
                fprintf(stderr, message); // avoid std::cerr since CRT callback
                return result;
            };

            // "warning C5039: '_CrtSetReportHook2': pointer or reference to 
            // potentially throwing function passed to extern C function under
            // -EHc. Undefined behavior may occur if this function throws an 
            // exception."
            // This is a false positive since report_cb lambda is noexcept and cannot throw.
            // Seems like MSVC do not warn about regular function, but less 
            // readable design, so keeping this suppression for now.
#pragma warning( push )            
#pragma warning( disable : 5039 ) 
            // Replace preinstalled hook with lambda and dump statistics
            _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, report_cb);
#pragma warning( pop )
            _CrtMemDumpAllObjectsSince(&pre_state_);
            _CrtMemDumpStatistics(&mem_diff);

            // Restore previous (preinstalled) hook
            if (preinstalled_hook)
            {
#pragma warning( push )            
#pragma warning( disable : 5039 ) // Same motivation as above
                _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, report_cb);
                _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, preinstalled_hook);
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

    if (test_info.result()->Passed() && leak_detected)
        GTEST_NONFATAL_FAILURE_("Memory leak detected");
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestProgramStart(
	const ::testing::UnitTest& /*test*/)
{
	// Empty for now
}

void gtest_memleak_detector::MemoryLeakDetectorListener::Impl::OnTestProgramEnd(
	const ::testing::UnitTest& /*test*/)
{
    in_.close();
	out_.close();

	std::remove(GTEST_MEMLEAK_DETECTOR_FILENAME);
	std::rename(GTEST_MEMLEAK_DETECTOR_TEMP_FILENAME, 
        GTEST_MEMLEAK_DETECTOR_FILENAME);
}

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_CRTDBG_AVAILABLE