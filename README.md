# gtest-memleak-detector
Google Test memory leak detection integration for C++11 projects and above.
Currently only works with MSVC tool-chain using 
[Microsoft CRT debug tools](https://docs.microsoft.com/en-us/visualstudio/debugger/crt-debugging-techniques?view=vs-2019).

## Features
- Automatic memory leak detection in test cases for code exercised within test case function.
- Memory leak reporting as Google Test failures so that tests fail if a leak is detected.
- Memory leak suppression so that memory leaks are not reported if the test fail due to failed assertion.
- Rerunning a failed test for a debug build with debugger attached will break at the allocation statement causing the leak.

## Requirements
The project depends on the open source [Google Test](https://github.com/google/googletest) project. 
You can import and add Google Test yourself or let the CMake script of this project download and 
build it for you by setting the CMake property GTEST_MEMLEAK_DETECTOR_DOWNLOAD_GTEST 
to ON (default). If you already have Google Test added to your project, set 
GTEST_POLICIES_DOWNLOAD_GTEST to OFF.

## Getting started
To start using the memory leak detector, simply add the project as a sub-directory 
to your existing CMake project. Then, include the gtest_memleak_detector header:

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

A complete example of the basic setup can be found in 
[example/01_getting_started](example/01_getting_started)

## Known Limitations
- It would make sense to make memory leak suppression in case of failed assertions optional,
  but at the time being it is mandatory since GTest allocates memory during assertion failures.

## License

This project is released under the MIT license, 
see [License](https://github.com/ekcoh/gtest-memleak-detector/blob/master/LICENSE).
This basically means you can do whatever you want with this project as long as you provide 
the original license and copyright information with your own project distribution.