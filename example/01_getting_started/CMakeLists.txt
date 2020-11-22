# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

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

