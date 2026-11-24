@echo off
setlocal
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
    echo Run from the x64 Native Tools Command Prompt for Visual Studio.
    exit /b 1
)
if not exist "%~1\Include\10.0.26100.0\km\ntddk.h" (
    echo Usage: build-driver.bat EXTRACTED_WDK_C_DIRECTORY [OUTPUT_DIRECTORY]
    exit /b 1
)
set "SOURCE=%~dp0"
set "WDK=%~f1"
set "TANITA_OUTPUT=%~2"
if not defined TANITA_OUTPUT set "TANITA_OUTPUT=%SOURCE%..\..\release\driver"
set "TANITA_OBJECTS=%SOURCE%..\build"
if not exist "%TANITA_OBJECTS%\." mkdir "%TANITA_OBJECTS%"
if not exist "%TANITA_OUTPUT%\." mkdir "%TANITA_OUTPUT%"
pushd "%TANITA_OUTPUT%" || exit /b 1
set "KIT=%WDK%\Lib\10.0.26100.0\km\x64"
set "WDF=%WDK%\Lib\wdf\kmdf\x64\1.15"
cl /nologo /W4 /WX /wd4324 /TC /std:c17 /kernel /GS /O2 /c /D_AMD64_ /D_WIN32_WINNT=0x0A00 /DNTDDI_VERSION=0x0A000000 /I"%WDK%\Include\10.0.26100.0\km" /I"%WDK%\Include\wdf\kmdf\1.15" "%SOURCE%Driver.c" /Fo:"%TANITA_OBJECTS%\Driver.obj"
if errorlevel 1 goto error
link /nologo /kernel /driver /subsystem:native,10.00 /entry:FxDriverEntry /nodefaultlib /machine:x64 /out:TanitaHid.sys "%TANITA_OBJECTS%\Driver.obj" /libpath:"%KIT%" /libpath:"%WDF%" ntoskrnl.lib hal.lib BufferOverflowK.lib wdfdriverentry.lib wdfldr.lib vhfkm.lib
if errorlevel 1 goto error
copy /y "%SOURCE%TanitaHid.inf" . >nul
if errorlevel 1 goto error
"%WDK%\tools\10.0.26100.0\x64\infverif.exe" /w TanitaHid.inf
if errorlevel 1 goto error
if exist TanitaHid.cat del /q TanitaHid.cat
"%WDK%\bin\10.0.26100.0\x86\Inf2Cat.exe" /driver:. /os:10_RS3_X64,10_VB_X64,10_CO_X64,10_NI_X64,10_GE_X64
if errorlevel 1 goto error
if not exist TanitaHid.cat goto error
popd
exit /b 0
:error
popd
exit /b 1
