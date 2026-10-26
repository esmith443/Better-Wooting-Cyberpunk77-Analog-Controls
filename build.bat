@echo off
setlocal

set "ROOT=%~dp0"
set "BUILD=%~1"
if "%BUILD%"=="" set "BUILD=%LOCALAPPDATA%\WootingAnalog\build"

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (
  echo Visual Studio with the C++ workload was not found.
  exit /b 1
)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set "CMAKE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
where cmake >nul 2>&1 && set "CMAKE=cmake"

"%CMAKE%" -S "%ROOT%." -B "%BUILD%" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_BUILD_TYPE=Release || exit /b 1
"%CMAKE%" --build "%BUILD%" || exit /b 1

echo.
echo Running unit tests...
"%BUILD%\UnitTests.exe" || exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\Package.ps1" -BuildDir "%BUILD%" || exit /b 1
endlocal
