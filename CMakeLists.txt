# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

cmake_minimum_required (VERSION 3.11)

###################################################################################################
# project: gtest_memleak_detector
###################################################################################################

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

if(MSVC)
	# CMake appends /W3 by default, and having /W3 followed by /W4 will result in 
	# cl : Command line warning D9025 : overriding '/W3' with '/W4'.  Since this is
	# a command line warning and not a compiler warning, it cannot be suppressed except
	# by fixing the command line.
	string(REGEX REPLACE " /W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
endif(MSVC)

target_include_directories(${PROJECT_NAME} 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(src)

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