@echo off
setlocal
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
    echo Run from the x64 Native Tools Command Prompt for Visual Studio.
    exit /b 1
)
if not exist "%~1\src\ViGEmClient.cpp" (
    echo Usage: build-app.bat PATH_TO_VIGEMCLIENT [OUTPUT_DIRECTORY]
    exit /b 1
)
set "SOURCE=%~dp0"
set "VIGEM=%~f1"
set "TANITA_OUTPUT=%~2"
if not defined TANITA_OUTPUT set "TANITA_OUTPUT=%SOURCE%..\release"
set "TANITA_OBJECTS=%SOURCE%build"
if not exist "%TANITA_OBJECTS%\." mkdir "%TANITA_OBJECTS%"
if not exist "%TANITA_OUTPUT%\." mkdir "%TANITA_OUTPUT%"
pushd "%TANITA_OUTPUT%" || exit /b 1
cl /nologo /W4 /WX /source-charset:.1252 /EHsc /std:c++20 /MT /DUNICODE /D_UNICODE /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 /I"%VIGEM%\include" /c "%SOURCE%App.cpp" /Fo:"%TANITA_OBJECTS%\App.obj"
if errorlevel 1 goto error
cl /nologo /W3 /EHsc /std:c++20 /source-charset:.1252 /MT /DUNICODE /D_UNICODE /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 /I"%VIGEM%\include" /c "%VIGEM%\src\ViGEmClient.cpp" /Fo:"%TANITA_OBJECTS%\ViGEmClient.obj"
if errorlevel 1 goto error
rc /nologo /i "%SOURCE%." /fo "%TANITA_OBJECTS%\Mapping.res" "%SOURCE%Mapping.rc"
if errorlevel 1 goto error
link /nologo /out:Tanita.exe /subsystem:windows,10.00 /manifest:embed /manifestdependency:"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'" "%TANITA_OBJECTS%\App.obj" "%TANITA_OBJECTS%\ViGEmClient.obj" "%TANITA_OBJECTS%\Mapping.res" user32.lib gdi32.lib hid.lib setupapi.lib advapi32.lib comctl32.lib shell32.lib gdiplus.lib shlwapi.lib
if errorlevel 1 goto error
copy /y "%VIGEM%\LICENSE" ViGEmClient-LICENSE.txt >nul
if errorlevel 1 goto error
copy /y "%SOURCE%..\LICENSE" LICENSE.txt >nul
if errorlevel 1 goto error
popd
exit /b 0
:error
popd
exit /b 1
