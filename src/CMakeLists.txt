# Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
# This file is subject to the license terms in the LICENSE file 
# found in the root directory of this distribution.

target_sources(${PROJECT_NAME}
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/memory_leak_detector_listener.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/memory_leak_detector_listener_impl.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/memory_leak_detector_listener_impl.h"
        "${CMAKE_CURRENT_LIST_DIR}/memory_leak_detector_stacktrace.cpp"
)