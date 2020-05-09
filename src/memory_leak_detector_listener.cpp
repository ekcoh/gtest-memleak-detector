// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution..

#include <gtest_memleak_detector/gtest_memleak_detector.h>

#include "memory_leak_detector_listener_impl.h"


#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
gtest_memleak_detector::MemoryLeakDetectorListener::MemoryLeakDetectorListener(
    int argc, char** argv)
    : impl_(nullptr)
{
	impl_ = std::make_unique<Impl>(argc, argv);
}
#else
gtest_memleak_detector::MemoryLeakDetectorListener::MemoryLeakDetectorListener(
    int, char**)
    : impl_(nullptr)
{ }
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

gtest_memleak_detector::MemoryLeakDetectorListener::~MemoryLeakDetectorListener()
{
	// Empty but required for PIMPL idiom
}

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestProgramStart(
	const ::testing::UnitTest& unit_test)
{
	impl_->OnTestProgramStart(unit_test);
}
#else
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestProgramStart(
	const ::testing::UnitTest&) {}
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestStart(
	const ::testing::TestInfo& test_info)
{
	impl_->OnTestStart(test_info);
}
#else
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestStart(
	const ::testing::TestInfo&) {}
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestEnd(
	const ::testing::TestInfo& test_info)
{
	impl_->OnTestEnd(test_info);
}
#else
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestEnd(
	const ::testing::TestInfo&) {}
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

#ifdef GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestProgramEnd(
	const ::testing::UnitTest& unit_test)
{
	impl_->OnTestProgramEnd(unit_test);
}
#else
void gtest_memleak_detector::MemoryLeakDetectorListener::OnTestProgramEnd(
	const ::testing::UnitTest&) {}
#endif // GTEST_MEMLEAK_DETECTOR_MEMORY_LISTENER_IMPL_AVAILABLE

