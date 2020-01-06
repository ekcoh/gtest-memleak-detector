# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

add_executable(${PROJECT_NAME}_unit_tests
	main.cpp
	memory_leak_detector_listener_test.cpp
)

target_link_libraries(${PROJECT_NAME}_unit_tests
	PRIVATE ${PROJECT_NAME}
	PUBLIC gtest_main
)

enable_testing()
add_test(NAME ${PROJECT_NAME}_unit_tests COMMAND ${PROJECT_NAME}_unit_tests )

