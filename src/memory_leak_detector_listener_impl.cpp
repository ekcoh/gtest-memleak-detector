// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#include "memory_leak_detector_listener_impl.h"

#include <fstream>
#include <exception>
#include <Windows.h>

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

// Translation unit globals
namespace {
    //_CRT_REPORT_HOOK chained_report_hook = nullptr;

    class scoped_crt_report_hook
    {
    public:
        explicit scoped_crt_report_hook(_CRT_REPORT_HOOK hook) noexcept
            : hook_(hook), preinstalled_hook_(_CrtGetReportHook())
        { 
            if (hook)
                _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, hook);
        }

        ~scoped_crt_report_hook() noexcept
        {
            if (hook_) 
                uninstall(hook_);
            if (preinstalled_hook_) 
                install(preinstalled_hook_);
        }
    private:
        static void install(_CRT_REPORT_HOOK hook) noexcept
        {
            if (hook)
                set_hook(_CRT_RPTHOOK_INSTALL, hook);
        }

        static void uninstall(_CRT_REPORT_HOOK hook) noexcept
        {
            if (hook) 
                set_hook(_CRT_RPTHOOK_REMOVE, hook);
        }

        static void set_hook(int mode, _CRT_REPORT_HOOK hook)
        {
            if (_CrtSetReportHook2(mode, hook) == -1)
            {
                if (mode == _CRT_RPTHOOK_INSTALL)
                    GTEST_FATAL_FAILURE_("Failed to install CRT RPT hook for memory leak detection");
                else
                    GTEST_FATAL_FAILURE_("Failed to uninstall CRT RPT hook for memory leak detection");
            }
        }

        _CRT_REPORT_HOOK hook_;
        _CRT_REPORT_HOOK preinstalled_hook_;
    };
}

std::string gtest_memleak_detector::MemoryLeakDetectorListener::Impl::describe_test(const ::testing::TestInfo& test_info)
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

bool gtest_memleak_detector::MemoryLeakDetectorListener::Impl::try_parse_alloc_no(long& dst, const char* str) noexcept
{
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
    strncpy_s(buffer, start + 1, len);
    auto allocation_no = strtol(buffer, nullptr, 10);
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
    /*TCHAR szFileName[MAX_PATH];
    GetModuleFileName(NULL, szFileName, MAX_PATH)
*/
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
        const auto arg = argv[i];
        out_ << arg << '\n';
    }

    if (IsDebuggerPresent())
    {
        in_.open(GTEST_MEMLEAK_DETECTOR_FILENAME);
        if (in_)
        {
            if (!read_and_compare(argc, argv))
                in_.close(); // failed or invalid
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
        return;

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
	// Avoid adding extra asserts if test is not passing anyway and the failing logic is the main failure.
	// Another reason to do this is that gtest report false positives when reporting assertion failures
	// due to memory allocation within gtest framework (bad).
    const auto test_passed = test_info.result()->Passed();

	// Create memory checkpoint post test and compute diff to detect leak(s)
    auto leak_detected = false;
    _CrtMemState mem_diff;
    if (test_passed)
    {
        _CrtMemState post_state;
        _CrtMemCheckpoint(&post_state);
        leak_detected = _CrtMemDifference(&mem_diff, &pre_state_, &post_state);
        if (leak_detected)
        {
            // Dump objects and statistics via redirect hook
            // This indirectly sets parsed_alloc_no
            std::cerr << "Failure: Memory leak detected.\n\n";
            
            // Make scoped RPT hook since we only need to capture statistics
            static auto preinstalled_hook = _CrtGetReportHook();
            static auto report_cb = [](int reportType, char* message, int* returnValue) -> int {
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

            // Replace preinstalled hook with lambda and dump statistics
            _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, report_cb);
            _CrtMemDumpAllObjectsSince(&pre_state_);
            _CrtMemDumpStatistics(&mem_diff);

            // Restore previous (preinstalled) hook
            if (preinstalled_hook)
                _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, preinstalled_hook);
        }
    }

	// Write test information to output file
	if (out_)
	{
        out_ << parsed_alloc_no << '\n';		
        out_ << describe_test(test_info) << '\n';
	}

    if (leak_detected)
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