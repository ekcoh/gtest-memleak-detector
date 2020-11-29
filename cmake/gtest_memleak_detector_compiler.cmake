function(gtest_memleak_detector_apply_compiler_settings GTEST_MEMLEAK_DETECTOR_TARGET)
    if(MSVC)
	    # CMake appends /W3 by default, and having /W3 followed by /W4 will result in 
	    # cl : Command line warning D9025 : overriding '/W3' with '/W4'.  Since this is
	    # a command line warning and not a compiler warning, it cannot be suppressed except
	    # by fixing the command line.
	    #string(REGEX REPLACE " /W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	    #string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	    target_compile_options(${GTEST_MEMLEAK_DETECTOR_TARGET} PRIVATE /Wall /WX)
        target_compile_options(${GTEST_MEMLEAK_DETECTOR_TARGET} PRIVATE 
            /wd4514 # unreferenced inline function has been removed
            /wd4571 # Visual C++ 7.1 catch(...) semantics changed
            /wd4623 # default constructor was implicitly defined as deleted
            /wd4625 # copy constructor was implicitly defined as deleted
            /wd4626 # assignment operator was implicitly deleted
            /wd4668 # undefined preprocessor macro, replacing with '0' for #if/#elif
            /wd4710 # function not inlined
            /wd4774 # sprintf_s format string enforcing strict string literal
            /wd4820 # padding added after data member
            /wd5026 # move constructor was implicitly deleted
            /wd5027 # move assignment operator was implicitly deleted
            /wd5045 # Spectre mitigation for memory load
            /wd4628 # gtest uses non ANSI C with /Ze
            /wd4619 # attempting to disable a warning that does not exist, C5039
        )
    else(MSVC)
	    # For now, since not supported, just use inherited settings
    endif(MSVC)    
endfunction()