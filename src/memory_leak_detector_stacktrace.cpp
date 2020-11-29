// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file
// found in the root directory of this distribution.

#include "memory_leak_detector.h"

#ifdef GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE

gtest_memleak_detector::StackTrace::StackTrace()
    : StackWalker(StackWalker::StackWalkOptions::RetrieveLine)
    , state(State::Scanning)
{ 
    Reset();
}

gtest_memleak_detector::StackTrace::State
gtest_memleak_detector::StackTrace::CurrentState() const noexcept
{
    return state;
}

const std::stringstream& 
gtest_memleak_detector::StackTrace::Stream() const noexcept
{
    return buffer;
}

const gtest_memleak_detector::Location& 
gtest_memleak_detector::StackTrace::GetLocation() const noexcept
{
    return location;
}

void 
gtest_memleak_detector::StackTrace::Reset(State reset_to_state)
{
    const static std::stringstream initial;

    buffer.str(std::string(4096 * 4, '\0'));
    buffer.clear();
    buffer.copyfmt(initial);

    location = Location();

    state = reset_to_state;
}

bool 
gtest_memleak_detector::StackTrace::Filter(const CallstackEntry& entry) noexcept
{   // Provide some custom VC library filtering to prettify stacktrace
    auto filter = false;
    if (entry.undName[0] != 0)
    {
        if (strcmp(entry.undName, "operator new") == 0)
            filter = true;
        else if (entry.lineFileName[0] == 0)
        {
            if (strcmp(entry.undName, "calloc_base") == 0)
                filter = true;
            else if (strcmp(entry.undName, "malloc_dbg") == 0)
                filter = true;
            else if (strcmp(entry.undName, "malloc") == 0)
                filter = true;
            else if (strcmp(entry.undName, "realloc_dbg") == 0)
                filter = true;
            else if (strcmp(entry.undName, "realloc") == 0)
                filter = true;
        }
    }
    return filter;
}

void 
gtest_memleak_detector::StackTrace::Format(CallstackEntry& entry)
{   // Format stack trace to string buffer
    if (entry.lineFileName[0] == 0)
    {
        buffer << "- 0x" << (LPVOID)entry.offset << " (";
        if (entry.moduleName[0] == 0)
            buffer << entry.moduleName;
        else
            buffer << "[module-name not available]";
        buffer << "): [filename not available]: ";
    }
    else
    {
        buffer << "- " << entry.lineFileName << " (" << entry.lineNumber << "): ";
    }
    if (entry.undFullName[0] != 0)
        buffer << entry.undFullName;
    else if (entry.undName[0] != 0)
        buffer << entry.undName;
    else if (entry.name[0] != 0)
        buffer << entry.name;
    buffer << '\n';

}

void 
gtest_memleak_detector::StackTrace::OnDbgHelpErr(
    LPCSTR szFuncName, DWORD gle, DWORD64 addr)
{   // Discard dbg help error message, this is expected for e.g. 
    // malloc/realloc/free etc.
    UNREFERENCED_PARAMETER(szFuncName);
    UNREFERENCED_PARAMETER(gle);
    UNREFERENCED_PARAMETER(addr);
}

void 
gtest_memleak_detector::StackTrace::HandleCallstackEntry(CallstackEntry& entry)
{
    switch (state)
    {
    case State::Scanning:
        if (entry.undName[0] != 0 &&
            strcmp(entry.undName, "GTestMemoryLeakDetector4ll0c470rh00k") == 0)
        {
            state = State::Capture;
        }
        break;

    case State::Capture:
        if (entry.undName[0] != 0)
        {
            if (Filter(entry))
                return;

            // Store first stack trace row as origin
            if (location.line == Location::invalid_line &&
                entry.lineFileName[0] != 0)
            {
                location.file = entry.lineFileName;
                location.line = entry.lineNumber;
            }

            // Truncate end of stack trace based on hitting test function body
            const auto len = strlen(entry.undName);
            if (len >= 10 && memcmp(entry.undName + len - 10, "::TestBody", 10) == 0)
                state = State::Completed;
        }

        Format(entry);
        break;

    case State::Completed: // Fall-through
    case State::Exception: // Fall-through
    default:
        break;
    }
}

#pragma warning(push)
#pragma warning(disable: 26812) // warning C26812: 'CallstackEntryType' unscoped enum
void 
gtest_memleak_detector::StackTrace::OnCallstackEntry(
    CallstackEntryType eType, CallstackEntry& entry)
#pragma warning(pop)
{
    if (eType != lastEntry && entry.offset != 0)
    {
        try
        {
            HandleCallstackEntry(entry);
        }
        catch (...)
        {
            state = State::Exception;
        }
    }
}

#endif // GTEST_MEMLEAK_DETECTOR_IMPL_AVAILABLE
