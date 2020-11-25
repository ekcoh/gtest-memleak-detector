#include <gtest_memleak_detector/gtest_memleak_detector.h>
#include <memory_leak_detector.h>

#include <algorithm>
#include <cctype>
#include <string>

using namespace gtest_memleak_detector;

namespace {
    const char* test_binary_path = "test.exe";
    char* argv[] = { "test.exe" };
}

std::string this_file = __FILE__;

class memory_leak_detector_test : public ::testing::Test
{
public:
    memory_leak_detector_test()
        : sut(1, argv)
    {

    }

    virtual ~memory_leak_detector_test() noexcept = default;
    memory_leak_detector_test(const memory_leak_detector_test&) = delete;
    memory_leak_detector_test& operator=(const memory_leak_detector_test&) = delete;
    memory_leak_detector_test(memory_leak_detector_test&&) = delete;
    memory_leak_detector_test& operator=(memory_leak_detector_test&&) = delete;

    static void SetUpTestCase()
    {
        std::transform(this_file.begin(), this_file.end(), this_file.begin(),
            [](char c) { return static_cast<char>(std::tolower(c)); });
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {

    }

    void Fail(long leak_alloc_no,
        const char* leak_file,
        unsigned long leak_line,
        const char* leak_trace)
    {
        ++fail_count;

        alloc_no = leak_alloc_no;
        file = leak_file;
        line = leak_line;
        trace = leak_trace;
    }

    void GivenFailCallbackSet()
    {
        sut.SetFailureCallback(
            [this](long n, const char* f, unsigned long l, const char* t)
        { this->Fail(n, f, l, t); });
    }

    void Reset()
    {
        alloc_no = -1;
        line = unsigned long(-1);
        file.clear();
        trace.clear();
        fail_count = 0;
    }

    long alloc_no = -1;
    unsigned long line = unsigned long(-1);
    std::string file;
    std::string trace;
    unsigned fail_count = 0;

    MemoryLeakDetector sut;
};

TEST_F(memory_leak_detector_test, 
    end__should_not_report_failure__if_not_leaking_and_test_has_no_assertion_failures)
{
    GivenFailCallbackSet();

    auto descriptor = []() { return std::string("some_test"); };
    sut.Start(descriptor);
    sut.End(descriptor, true); // true: passed
    
    EXPECT_EQ(fail_count, 0u);
}

TEST_F(memory_leak_detector_test,
    end__should_not_report_failure__if_leaking_but_test_has_assertion_failures)
{
    GivenFailCallbackSet();

    auto descriptor = []() { return std::string("some_test"); };
    sut.Start(descriptor);
    auto* ptr = malloc(64);
    sut.End(descriptor, false); // false: not passed
    free(ptr); // cleanup

    EXPECT_EQ(fail_count, 0u);
}

TEST_F(memory_leak_detector_test,
    end__should_report_failure__if_leaking_and_test_has_no_assertion_failures)
{
    GivenFailCallbackSet();

    auto test_case = []() { return malloc(64); };

    auto descriptor = []() { return std::string("some_test"); };
    sut.Start(descriptor);
    auto* ptr = test_case();
    sut.End(descriptor, true);          // true: passed
    free(ptr);                          // cleanup

    ASSERT_EQ(fail_count, 1u);
    EXPECT_GT(alloc_no, 0);             // weak
    EXPECT_EQ(line, unsigned long(-1)); // no line
    EXPECT_STREQ(file.c_str(), "");     // first run, no trace info
    EXPECT_STREQ(trace.c_str(), "");    // first run, no trace info
}

unsigned long leaking_test_case_line = 0;
unsigned long test_line = 0;

__declspec(noinline) void* leaking_test_case(size_t size_bytes)
{
    leaking_test_case_line = unsigned long(__LINE__) + 1;
    auto* ptr = malloc(size_bytes);
    return ptr;
}

inline std::string make_trace_line(const std::string& file, unsigned long line, const std::string& function)
{
    return "- " + file + " (" + std::to_string(line) + "): " + function + "\n";
}

TEST_F(memory_leak_detector_test,
    end__should_report_trace__if_leaking_and_test_has_no_assertion_failures_and_database_have_already_been_populated)
{
    GivenFailCallbackSet();

    auto descriptor = []() { return std::string("some_test"); };
    sut.Start(descriptor);
    auto* ptr = leaking_test_case(64);
    sut.End(descriptor, true);          // true: passed
    free(ptr);                          // cleanup

    // Rerun to obtain stack trace
    Reset();
    sut.Start(descriptor);
    test_line = unsigned long(__LINE__) + 1;
    ptr = leaking_test_case(64);
    sut.End(descriptor, true);          // true: passed
    free(ptr);                          // cleanup

    std::string expected_trace = 
        make_trace_line(this_file, leaking_test_case_line, "leaking_test_case") +
        make_trace_line(this_file, test_line, __FUNCTION__);

    ASSERT_EQ(fail_count, 1u);
    EXPECT_GT(alloc_no, 0);                                 // weak
    EXPECT_EQ(line, leaking_test_case_line);                // test case line no
    EXPECT_STREQ(file.c_str(), this_file.c_str());          // first run, no trace info
    EXPECT_STREQ(trace.c_str(), expected_trace.c_str());    // first run, no trace info
}

#define GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART \
    "Memory leak detected."
#define GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART \
    "\n- Allocation request no: "
#define GTEST_MEMLEAK_DETECTOR_ORIGIN_MSG_PART \
    "\n- Origin: "
#define GTEST_MEMLEAK_DETECTOR_STACKTRACE_MSG_PART \
    "\n\nStacktrace:\n"
#define GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART \
    "\n\n(Re-run the test again to obtain stack-trace for the allocation " \
    "causing the memory-leak.)"

TEST_F(memory_leak_detector_test,
    make_database_file_path__should_return_same_path_with_additional_suffix__if_given_valid_string)
{
    EXPECT_STREQ(MemoryLeakDetector::MakeDatabaseFilePath("test.exe").c_str(),
        "test.exe.gt.memleaks");
    EXPECT_STREQ(MemoryLeakDetector::MakeDatabaseFilePath("c:\\something\\test.exe").c_str(),
        "c:\\something\\test.exe.gt.memleaks");
    EXPECT_STREQ(MemoryLeakDetector::MakeDatabaseFilePath("/user/myuser/test").c_str(),
        "/user/myuser/test.gt.memleaks");
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_all_info__if_given_only_valid_input)
{
    EXPECT_STREQ(MemoryLeakDetector::MakeFailureMessage(
        1234, "somefile.cpp", 67, "stacktrace_data").c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART "1234"
        GTEST_MEMLEAK_DETECTOR_ORIGIN_MSG_PART "somefile.cpp: 67"
        GTEST_MEMLEAK_DETECTOR_STACKTRACE_MSG_PART "stacktrace_data");
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_only_info_about_leak__if_given_all_valid_input_except_stacktrace)
{
    EXPECT_STREQ(MemoryLeakDetector::MakeFailureMessage(
        3456, "somefile.cpp", 51, nullptr).c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART "3456"
        GTEST_MEMLEAK_DETECTOR_ORIGIN_MSG_PART "somefile.cpp: 51"
        GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART);
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_only_info_and_stacktrace__if_given_all_valid_input_except_filename_and_line)
{
    EXPECT_STREQ(MemoryLeakDetector::MakeFailureMessage(
        3456, nullptr, 0, "stacktrace_data").c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART "3456"
        GTEST_MEMLEAK_DETECTOR_STACKTRACE_MSG_PART "stacktrace_data");
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_only_allocation_request__if_all_strings_are_nullptr_but_valid_alloc_no)
{
    EXPECT_STREQ(MemoryLeakDetector::MakeFailureMessage(3456, nullptr, 0, nullptr).c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_REQUEST_MSG_PART "3456"
        GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART);
}

TEST_F(memory_leak_detector_test,
    make_failure_message__should_return_message_containing_only_leak_info__if_all_strings_are_nullptr_and_invalid_alloc_no)
{
    EXPECT_STREQ(MemoryLeakDetector::MakeFailureMessage(
        -1, nullptr, 0, nullptr).c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART);
    EXPECT_STREQ(MemoryLeakDetector::MakeFailureMessage(
        -2, nullptr, 0, nullptr).c_str(),
        GTEST_MEMLEAK_DETECTOR_LEAK_MSG_PART
        GTEST_MEMLEAK_DETECTOR_RERUN_MESSAGE_PART);
}