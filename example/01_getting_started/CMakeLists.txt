cmake_minimum_required (VERSION 3.6)

add_executable(${PROJECT_NAME}_example_01_getting_started
	"main.cpp"
)

target_link_libraries(${PROJECT_NAME}_example_01_getting_started
	PRIVATE ${PROJECT_NAME}
)

target_link_libraries(${PROJECT_NAME}_example_01_getting_started
	PRIVATE gtest_main
)

enable_testing()
add_test(NAME ${PROJECT_NAME}_example_01_getting_started 
	COMMAND ${PROJECT_NAME}_example_01_getting_started
)

# TODO If build as shared library, copy library to current binary folder