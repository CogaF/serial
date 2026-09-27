@echo off
rem Builds everything: serial.lib and serial.dll, Debug and Release, x64 and x86.
rem Each build also runs the smoke tests; the script stops at the first failure.
rem
rem   build_all.bat              -> Builds\          (C runtime /MD, /MDd - the Visual Studio default)
rem   build_all.bat static-crt   -> Builds_StaticCRT\ (C runtime /MT, /MTd)
rem
rem Works from any Command Prompt: Visual Studio (2022 or later) is found with vswhere.
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "EXTRA="
set "OUT=Builds"
if /i "%~1"=="static-crt" (
    set "EXTRA=/p:SerialStaticRuntime=true"
    set "OUT=Builds_StaticCRT"
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Installer not found ^(vswhere.exe^). Install Visual Studio 2022 or later with "Desktop development with C++".
    exit /b 1
)
set "MSBUILD="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    if not defined MSBUILD set "MSBUILD=%%i"
)
if not defined MSBUILD (
    echo MSBuild not found. Install Visual Studio 2022 or later with "Desktop development with C++".
    exit /b 1
)
echo Using %MSBUILD%

for %%p in (x64 x86) do (
    for %%c in (Debug Release) do (
        echo.
        echo ===== %%c ^| %%p =====
        "%MSBUILD%" visual_studio\visual_studio.sln /m /nologo /verbosity:minimal /t:Build /p:Configuration=%%c /p:Platform=%%p %EXTRA%
        if errorlevel 1 (
            echo.
            echo BUILD FAILED: %%c ^| %%p
            exit /b 1
        )
    )
)

echo.
echo All builds succeeded. Results in %OUT%\:
for %%p in (x64 x86) do for %%c in (Debug Release) do (
    echo   %OUT%\%%p\%%c\lib\serial.lib
    echo   %OUT%\%%p\%%c\dll\serial.dll  + serial.lib ^(import library^) + serial.pdb
)
echo   %OUT%\include\serial\serial.h, v8stdint.h
exit /b 0
