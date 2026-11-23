@echo off
setlocal
where cl.exe >nul 2>&1
if errorlevel 1 (
    echo Run this script from the x64 Native Tools Command Prompt for Visual Studio.
    exit /b 1
)
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
    echo Select the x64 MSVC toolchain before building.
    exit /b 1
)
set "TANITA_OUTPUT=%~1"
if not defined TANITA_OUTPUT set "TANITA_OUTPUT=%~dp0..\release"
set "TANITA_OBJECTS=%~dp0build"
if not exist "%TANITA_OBJECTS%\." mkdir "%TANITA_OBJECTS%"
if not exist "%TANITA_OUTPUT%\." mkdir "%TANITA_OUTPUT%"
if not exist "%TANITA_OUTPUT%\." exit /b 1
cl /nologo /W4 /WX /EHsc /std:c++20 /MT /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 "%~dp0Probe.cpp" /Fo:"%TANITA_OBJECTS%\Probe.obj" /Fe:"%TANITA_OUTPUT%\Probe.exe" /link user32.lib hid.lib /MACHINE:X64 /SUBSYSTEM:CONSOLE,10.00
exit /b %errorlevel%
