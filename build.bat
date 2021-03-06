@rem Copyright(C) 2019 - 2020 H�kan Sidenvall <ekcoh.git@gmail.com>.
@rem This file is subject to the license terms in the LICENSE file found in the 
@rem root directory of this distribution.

@rem Build script for building with MSVC in Windows environment
@rem 
@rem Preconditions:
@rem - The environment has already been setup, i.e. VC variables set, e.g.
@rem   run for Visual Studio 2017 from cmd: 
@rem   "E:\Program\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
@rem - CMake is available
@rem - Ninja is available

:: Main build flow
@setlocal
@set binary_dir="build/x64-debug"
@if not exist %binary_dir% mkdir %binary_dir%
@set starttime=%TIME%
@rem echo Build start time: %starttime%
@cd %binary_dir%
@call :generate
@if %errorlevel% neq 0 @echo. && @echo Build failed. && @exit /B #%errorlevel%
@call :build
@if %errorlevel% neq 0 @echo. && @echo Build failed. && @exit /B #%errorlevel%
@call :test
@if %errorlevel% neq 0 @echo. && @echo Build failed. && @exit /B #%errorlevel%
@goto done

:: Make directory
:chdir_build
@set binary_dir="build/x64-debug"
@if not exist %binary_dir% mkdir %binary_dir%
@pushd %binary_dir%

:: Run CMake generator
:generate
cmake -G "Ninja" ../.. -DCMAKE_VERBOSE_MAKEFILE=OFF -DBUILD_SHARED_LIBS=OFF
@exit /B #%errorlevel%

:: Run CMake build
:: Note that Debug is default target since tool is useless in Release mode
:build
cmake --build . --config Debug --target all 
@exit /B #%errorlevel%

:: Run CTest
:test
ctest
@exit /B #%errorlevel%

:usage
@goto end

:error
@echo %1%
@goto end

:: Print elapsed time as part of terminating the script
:done
@set endtime=%TIME%
@rem @echo Build end time: %endtime%
@echo off
for /F "tokens=1-4 delims=:.," %%a in ("%STARTTIME%") do (
    set /A "start=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
for /F "tokens=1-4 delims=:.," %%a in ("%ENDTIME%") do (
    set /A "end=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
@echo on
@set /A elapsed=end-start
@set /A hh=elapsed/(60*60*100), rest=elapsed%%(60*60*100), mm=rest/(60*100), rest%%=60*100, ss=rest/100, cc=rest%%100
@if %hh% lss 10 set hh=0%hh%
@if %mm% lss 10 set mm=0%mm%
@if %ss% lss 10 set ss=0%ss%
@if %cc% lss 10 set cc=0%cc%
@set DURATION=%hh%:%mm%:%ss%,%cc%
@echo.
@echo Build succeeded. Time: %DURATION%

:: End script successfully
:end
@endlocal
@exit /B 0