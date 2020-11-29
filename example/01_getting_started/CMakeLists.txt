# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

include(GoogleTest)

add_executable(${PROJECT_NAME}_example_01_getting_started
	"main.cpp"
)
gtest_memleak_detector_apply_compiler_settings(${PROJECT_NAME}_example_01_getting_started)

target_link_libraries(${PROJECT_NAME}_example_01_getting_started
	PRIVATE ${PROJECT_NAME}
	PRIVATE gtest_main
)

if (${PROJECT_NAME_UCASE}_ADD_EXAMPLE_TESTS)
    enable_testing()
	gtest_discover_tests(${PROJECT_NAME}_example_01_getting_started)
	
endif()

