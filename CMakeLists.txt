# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

cmake_minimum_required (VERSION 3.11)

###################################################################################################
# project: gtest_memleak_detector
###################################################################################################

include(FetchContent)
include(cmake/gtest_memleak_detector_compiler.cmake)

project(gtest_memleak_detector 
    VERSION 0.1.1
	HOMEPAGE_URL https://github.com/ekcoh/gtest-memleak-detector.git
	LANGUAGES CXX
)
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UCASE)

option(${PROJECT_NAME_UCASE}_BUILD_TESTS 
	"If enabled, compile the tests." ON)
option(${PROJECT_NAME_UCASE}_BUILD_EXAMPLES 
	"If enabled, compile the examples." ON)
option(${PROJECT_NAME_UCASE}_ADD_EXAMPLE_TESTS 
	"If enabled, adds the example tests as part of CTest suite." OFF)
option(${PROJECT_NAME_UCASE}_DOWNLOAD_DEPENDENCIES 
	"If enabled, download dependencies." ON)

if (${PROJECT_NAME_UCASE}_DOWNLOAD_DEPENDENCIES)
    ###############################################################################################
    # Download and unpack StackWalker at configure time if not already available.
    # If made available by parent project use that version and configuration instead.
    FetchContent_Declare(
      stackwalker
      GIT_REPOSITORY https://github.com/JochenKalmbach/StackWalker.git
      GIT_TAG        1.20
    )
    FetchContent_GetProperties(stackwalker)
    if(NOT stackwalker_POPULATED)
      FetchContent_Populate(stackwalker)
      set(StackWalker_DISABLE_TESTS ON CACHE BOOL "" FORCE)
      add_subdirectory(${stackwalker_SOURCE_DIR} ${stackwalker_BINARY_DIR})
    endif()

    ###############################################################################################
    # Download and unpack Google Test at configure time if not already available.
    # If made available by parent project use that version and configuration instead.
    # Currently using release 1.10.0 (October 2019)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        release-1.10.0
    )
    FetchContent_GetProperties(googletest)
    if(NOT googletest_POPULATED)
      FetchContent_Populate(googletest)
      if (WIN32)
        # Prevent overriding the parent project's compiler/linker
        # settings on Windows
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
      endif()
      add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
    endif()
endif()

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
	PRIVATE ${stackwalker_SOURCE_DIR}/Main # Incorrectly setup by StackWalker CMake
)
target_link_libraries(${PROJECT_NAME}
	PRIVATE gtest_main
	PRIVATE StackWalker
)
target_compile_options(${PROJECT_NAME} 
    PRIVATE /wd4711 # automatic inline expansion (optimized)
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