[![Build status](https://ci.appveyor.com/api/projects/status/i7uqdte77kny1bge?svg=true)](https://ci.appveyor.com/project/ekcoh/gtest-memleak-detector)

# gtest-memleak-detector
Google Test memory leak detection integration for C++11 projects and above.
Currently only works with MSVC tool-chain using
[Microsoft CRT debug tools](https://docs.microsoft.com/en-us/visualstudio/debugger/crt-debugging-techniques?view=vs-2019).
Provides stack-traces for memory leak origins that are hyperlinked when using Google Test Adapter in Visual Studio.

## Features
- Automatic memory leak detection in test cases for code exercised within test case function.
- Memory leak reporting as Google Test failures so that tests fail if a leak is detected.
- Automatic memory leak report suppression so that memory leaks are not reported if the test fail due to a more severe failed assertion.
- All memory leak failures contain allocation request number obtained from allocation hook.
- Rerunning a failed test will provide a filtered stack-trace for the origin of the allocation causing the leak.
- Coexistence support for other CRTDBG allocation hooks and reporting hooks to be installed at the same time.
- Support for leak detection via malloc, realloc, new (Same as CRTDBG supports).
- If the code exercised by a test case has multiple leaks, only the first leak is reported.

## Requirements
The project depends on the open source [Google Test](https://github.com/google/googletest) and
[StackWalker](https://github.com/JochenKalmbach/StackWalker) projects. 
Dependencies are managed via CMake FetchContent (requires CMake 3.11+) which means that if a 
parent project already includes it that version will be used instead of predefined versions. 
By default, this project will fetch dependencies automatically from GitHub if 
`GTEST_MEMLEAK_DETECTOR_DOWNLOAD_DEPENDENCIES` is `ON` (default). 
If `GTEST_MEMLEAK_DETECTOR_DOWNLOAD_DEPENDENCIES` is `OFF`, you have to manually make 
Google Test and StackWalker available to the build.

## Getting started
To start using the memory leak detector, simply add the project as a sub-directory 
to your existing CMake project and as a dependency to your test binary via `target_link_libraries()`. 
Finally, include the gtest_memleak_detector header in your Google Test main file as illustrated below:

```cpp
#include <gtest_memleak_detector/gtest_memleak_detector.h>  // Include memory leak detector
```

In the main function of the test program, directly after initializing Google Test, 
add memory leak detection support by adding it as a test event listener, e.g:

```cpp
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::UnitTest::GetInstance()->listeners().Append(
    new gtest_memleak_detector::MemoryLeakDetectorListener(argc, argv));
  return RUN_ALL_TESTS();
}
```
...or use one of the convenience macros:

```cpp
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  GTEST_MEMLEAK_DETECTOR_APPEND_LISTENER;
  return RUN_ALL_TESTS();
}
```

...or even shorter:

```cpp
GTEST_MEMLEAK_DETECTOR_MAIN
```

No changes are required to existing test code except changes explained above.

A complete example of the basic setup can be found in 
[example/01_getting_started](example/01_getting_started).

## Known Limitations
- It would make sense to make memory leak suppression in case of failed assertion optional,
  but it has to be suppressed since GTest allocates memory during assertion failures and
  this would otherwise be reported as a false positive.
- Only ANSI filenames are currently supported. This means that proper UNICODE support is currently missing.
- Leaks caused by alternative memory allocation functions, e.g. HeapAlloc in WINAPI, will not be reported since this is not supported by CRTDBG.

## CMake Options

The following CMake options are supported:

Option                                        | Default Value | Description
--------------------------------------------- | ------------- | ---------------------------------------------------------------------------------------------
GTEST_MEMLEAK_DETECTOR_BUILD_TESTS            | ON            | If `ON`, builds the verification test binaries and adds them as part of the CTest test suite. 
GTEST_MEMLEAK_DETECTOR_BUILD_EXAMPLES         | ON            | If `ON`, builds the example test binaries.
GTEST_MEMLEAK_DETECTOR_ADD_EXAMPLE_TESTS      | OFF           | If `ON`, includes example tests (some intentionally failing) as part of the CTest test suite. 
GTEST_MEMLEAK_DETECTOR_DOWNLOAD_DEPENDENCIES  | ON            | If `ON`, automatically fetches online third-party dependencies.

## License

This project is released under the MIT license, 
see [License](https://github.com/ekcoh/gtest-memleak-detector/blob/master/LICENSE).
This basically means you can do whatever you want with this project as long as you provide 
the original license and copyright information with your own project distribution.

## Third Party Licenses

Third party dependencies (not distributed, but may indirectly be downloaded via CMake) are licensed under the following licenses:
- [Google Test](https://github.com/google/googletest): [BSD-3-Clause License](https://github.com/google/googletest/blob/master/LICENSE)
- [StackWalker](https://github.com/JochenKalmbach/StackWalker): [BSD-2-Clause License](https://github.com/JochenKalmbach/StackWalker/blob/master/LICENSE)