# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

cmake_minimum_required (VERSION 3.11)

###################################################################################################
# project: gtest_memleak_detector
###################################################################################################

include(cmake/gtest_memleak_detector_compiler.cmake)

project(gtest_memleak_detector 
    VERSION 0.1.0
	HOMEPAGE_URL https://github.com/ekcoh/gtest-memleak-detector.git
	LANGUAGES CXX
)
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UCASE)

option(${PROJECT_NAME_UCASE}_BUILD_TESTS 
	"If enabled, compile the tests." ON)
option(${PROJECT_NAME_UCASE}_BUILD_EXAMPLES 
	"If enabled, compile the examples." ON)

###################################################################################################
# library
###################################################################################################

# We want to build this as static to use debug heap
add_library(${PROJECT_NAME} STATIC
	"include/gtest_memleak_detector/gtest_memleak_detector.h"
)
add_subdirectory(src)
gtest_memleak_detector_apply_compiler_settings(${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME}
	PRIVATE gtest_main
)

###################################################################################################
# tests
###################################################################################################

if (${PROJECT_NAME_UCASE}_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
endif(${PROJECT_NAME_UCASE}_BUILD_TESTS)

###################################################################################################
# examples
###################################################################################################

if (${PROJECT_NAME_UCASE}_BUILD_EXAMPLES)
	add_subdirectory(example)
endif(${PROJECT_NAME_UCASE}_BUILD_EXAMPLES)