@echo off
rem Генерирует решение Visual Studio 2022 в папке build\
setlocal

set "CMAKE=cmake"
where cmake >nul 2>nul && goto generate

rem CMake не в PATH: берём тот, что поставляется с Visual Studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSDIR=%%i"
set "CMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

:generate
"%CMAKE%" -S "%~dp0." -B "%~dp0build" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

echo.
echo Solution: %~dp0build\Sonder.sln
