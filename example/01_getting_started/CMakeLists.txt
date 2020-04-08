# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

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

add_executable(${PROJECT_NAME}_example_01_getting_started
	"main.cpp"
)

target_link_libraries(${PROJECT_NAME}_example_01_getting_started
	PRIVATE ${PROJECT_NAME}
)

target_link_libraries(${PROJECT_NAME}_example_01_getting_started
	PRIVATE gtest_main
)

option(${PROJECT_NAME_UCASE}_ADD_EXAMPLE_TESTS 
	"If enabled, adds the example tests as part of CTest suite." OFF)

if (${PROJECT_NAME_UCASE}_ADD_EXAMPLE_TESTS)
    enable_testing()
    add_test(NAME ${PROJECT_NAME}_example_01_getting_started 
	    COMMAND ${PROJECT_NAME}_example_01_getting_started
    )
endif()

