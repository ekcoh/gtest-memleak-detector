# Copyright(C) 2019 - 2020 H�kan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

target_sources(${PROJECT_NAME}
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/memory_leak_detector_listener.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/memory_leak_detector.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/memory_leak_detector.h"
        "${CMAKE_CURRENT_LIST_DIR}/memory_leak_detector_stacktrace.cpp"
)