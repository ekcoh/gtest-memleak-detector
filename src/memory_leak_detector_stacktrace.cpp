// Copyright(C) 2019 - 2020 H�kan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file
// found in the root directory of this distribution.

#include "memory_leak_detector_listener_impl.h"

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

gtest_memleak_detector::StackTrace::StackTrace(
    Buffer* buffer, Location* location, const char* hook_file)
    : StackWalker(StackWalker::StackWalkOptions::RetrieveLine)
    , buffer(buffer)
    , location(location)
    , hook_file(hook_file)
    , state(State::Pre)
    , suppress(true)
{ 
    assert(buffer);
    assert(buffer->data[0] == 0);
    assert(buffer->last == buffer->data);
    assert(buffer->end > buffer->data);
    
    assert(location);
    assert(location->file[0] == 0);
    assert(location->line == Location::invalid_line);
}

void gtest_memleak_detector::StackTrace::Copy(char* dst, const char* src) noexcept
{
    assert(dst);
    assert(src);
    static_assert(_MSC_VER > 1400, "Requires MSVC version 2008 or newer");
    (void)strncpy_s(dst, STACKWALK_MAX_NAMELEN, src, _TRUNCATE);
}

bool gtest_memleak_detector::StackTrace::Filter(const CallstackEntry& entry) noexcept
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

void gtest_memleak_detector::StackTrace::Format(CallstackEntry& entry)
{
    const size_t maxCount = static_cast<size_t>(buffer->end - buffer->last - 1);
    int offset = 0;
    if (entry.name[0] == 0)
        Copy(entry.name, "(function-name not available)");
    if (entry.undName[0] != 0)
        Copy(entry.name, entry.undName);
    if (entry.undFullName[0] != 0)
        Copy(entry.name, entry.undFullName);
    if (entry.lineFileName[0] == 0)
    {
        Copy(entry.lineFileName, "(filename not available)");
        if (entry.moduleName[0] == 0)
            Copy(entry.moduleName, "(module-name not available)");
        offset = _snprintf_s(buffer->last, maxCount, maxCount, "- 0x%p (%s): %s: %s\n",
            (LPVOID)entry.offset, entry.moduleName, entry.lineFileName, entry.name);
    }
    else
    {
        offset = _snprintf_s(buffer->last, maxCount, maxCount, "- %s (%lu): %s\n",
            entry.lineFileName, entry.lineNumber, entry.name);
    }
    if (offset > 0)
        buffer->last += offset;
}

void gtest_memleak_detector::StackTrace::OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr)
{   // Discard dbg help error message, this is expected for e.g. malloc/realloc/free
    UNREFERENCED_PARAMETER(szFuncName);
    UNREFERENCED_PARAMETER(gle);
    UNREFERENCED_PARAMETER(addr);
}

void gtest_memleak_detector::StackTrace::OnCallstackEntry(CallstackEntryType eType, CallstackEntry& entry)
{
    if (eType == lastEntry || entry.offset == 0)
        return;

    switch (state)
    {
    case State::Pre:
        // First stack trace hit of relevance is AllocHook
        if (_stricmp(entry.lineFileName, hook_file) == 0)
            state = State::Active;
        break;
    case State::Post:
        break;
    case State::Active:
        if (entry.undName[0] != 0)
        {
            // Abort if filtered stack trace
//#ifndef GTEST_MEMLEAK_DETECTOR_DISABLE_FILTER_STACKTRACE
            if (Filter(entry))
                return;
            //#endif // GTEST_MEMLEAK_DETECTOR_DISABLE_FILTER_STACKTRACE

                        // Store first stack trace row as origin
            if (location->line == Location::invalid_line && entry.lineFileName[0] != 0)
            {
                strncpy_s(location->file, max_path,
                    entry.lineFileName, max_path - 1);
                location->file[max_path - 1] = 0;
                location->line = entry.lineNumber;
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

#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE