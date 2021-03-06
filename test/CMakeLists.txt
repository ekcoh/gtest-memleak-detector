# Copyright(C) 2019 - 2020 H�kan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

cmake_minimum_required (VERSION 3.11)

include(FetchContent)
include(GoogleTest)

enable_testing()

###################################################################################################
# Unit tests
add_executable(${PROJECT_NAME}_unit_tests
	main.cpp
	memory_leak_detector_listener_test.cpp
	memory_leak_detector_listener_assertion_test.cpp
    memory_leak_detector_test.cpp
)
gtest_memleak_detector_apply_compiler_settings(${PROJECT_NAME}_unit_tests)
target_link_libraries(${PROJECT_NAME}_unit_tests
	PRIVATE ${PROJECT_NAME}
    PRIVATE StackWalker
	PUBLIC gtest
)
target_include_directories(${PROJECT_NAME}_unit_tests
    PRIVATE "../src"
    PRIVATE ${stackwalker_SOURCE_DIR}/Main # Incorrectly setup by StackWalker CMake
)
target_compile_options(${PROJECT_NAME}_unit_tests
	PRIVATE /FC     # Full Path of Source Code File in Diagnostics
	PRIVATE /wd4711 # automatic inline expansion (optimized - Release)
)
gtest_discover_tests(${PROJECT_NAME}_unit_tests)

###################################################################################################
# CRTDBG Coexistence tests to verify detector is operational if other CRTDBG hooks are active
add_executable(${PROJECT_NAME}_coexistence_tests
	main.cpp
	memory_leak_detector_listener_test.cpp
	memory_leak_detector_listener_coexistence_test.cpp
)
gtest_memleak_detector_apply_compiler_settings(${PROJECT_NAME}_coexistence_tests)
add_definitions(-DINCLUDE_CRT)
target_compile_options(${PROJECT_NAME}_coexistence_tests
    PRIVATE /wd4711 # automatic inline expansion (optimized - Release)
)
target_link_libraries(${PROJECT_NAME}_coexistence_tests
	PRIVATE ${PROJECT_NAME}
	PUBLIC gtest
)
gtest_discover_tests(${PROJECT_NAME}_coexistence_tests)
