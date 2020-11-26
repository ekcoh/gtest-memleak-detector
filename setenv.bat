@WHERE /q vswhere
@IF %ERRORLEVEL% NEQ 0 choco install vswhere
@vswhere.exe -latest -property installationPath > vsinstalldir.txt
@set /p VSINSTALLDIR=<vsinstalldir.txt
@del vsinstalldir.txt
@set VSAUXDIR=%VSINSTALLDIR%\VC\Auxiliary\Build
@rem VC\Auxiliary\Build
@rem @set VCVARS64_SCRIPT="%VSAUXDIR%\vcvars64.bat"
@echo VC Auxillary Directory: %VSAUXDIR%
@pushd %VSAUXDIR%
call vcvars64.bat
@popd
@rem for /f %%i in ('vswhere.exe -latest -property installationPath') do set VSINSTALLDIR=%%i
@rem CMD /C longjob.cmd >result.txt 2>&1
@rem C:\Program Files (x86)\Microsoft Visual Studio\Installer
@rem SETLOCAL ENDLOCAL
@rem vswhere -version [15.0, 16.0) -property installationPath