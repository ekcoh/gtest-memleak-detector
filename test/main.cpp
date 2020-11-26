// Copyright(C) 2019 - 2020 Håkan Sidenvall <ekcoh.git@gmail.com>.
// This file is subject to the license terms in the LICENSE file 
// found in the root directory of this distribution.

#ifdef INCLUDE_CRT
// Memory debugging tools (MSVC only)
#if defined(_DEBUG) && defined(_MSC_VER)

// CRT debug tools
#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif // _CRTDBG_MAP_ALLOC

#include <stdlib.h>
#include <crtdbg.h>

#endif // defined(_DEBUG) && defined(_MSC_VER)
#endif // INCLUDE_CRT

#pragma warning(push)
#pragma warning(disable: 26812) // MSVC C26812: unscoped enum
#pragma warning(disable: 26495) // MSVC C26495: unitialized variable
#include <gtest/gtest.h> // Google Test
#pragma warning(pop)

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}