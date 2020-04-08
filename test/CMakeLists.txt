# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

cmake_minimum_required (VERSION 3.11)

include(FetchContent)

###################################################################################################
# Download and unpack Google Test at configure time if not already available.
# If made available by parent project use that version and configuration instead.
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

###################################################################################################
# Unit tests
add_executable(${PROJECT_NAME}_unit_tests
	main.cpp
	memory_leak_detector_test.cpp
	memory_leak_detector_assertion_tests.cpp
)

target_link_libraries(${PROJECT_NAME}_unit_tests
	PRIVATE ${PROJECT_NAME}
	PUBLIC gtest_main
)

enable_testing()
add_test(NAME ${PROJECT_NAME}_unit_tests COMMAND ${PROJECT_NAME}_unit_tests )

# Coexistence tests
add_executable(${PROJECT_NAME}_coexistence_tests
	main.cpp
	memory_leak_detector_test.cpp
	memory_leak_detector_coexistence_test.cpp
)

add_definitions(-DINCLUDE_CRT)

target_link_libraries(${PROJECT_NAME}_coexistence_tests
	PRIVATE ${PROJECT_NAME}
	PUBLIC gtest_main
)

enable_testing()
add_test(NAME ${PROJECT_NAME}_coexistence_tests COMMAND ${PROJECT_NAME}_coexistence_tests )