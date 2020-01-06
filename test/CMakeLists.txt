# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

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