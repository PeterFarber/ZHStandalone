@echo off
setlocal EnableExtensions

cd /d "%~dp0\.."
set "ROOT=%CD%"

rem ---------------------------------------------------------------------------
rem Raylib quickstart style: Premake generates GNU Makefiles at repo root,
rem toolchain from PATH (recommended: https://github.com/skeeto/w64devkit)
rem Local toolchain: %USERPROFILE%\Documents\CPP\w64devkit (outside repo)
rem ---------------------------------------------------------------------------

set "W64DEVKIT=%USERPROFILE%\Documents\CPP\w64devkit"
if exist "%W64DEVKIT%\bin\" (
  set "PATH=%W64DEVKIT%\bin;%PATH%"
  echo [build.bat] Prepended toolchain: "%W64DEVKIT%\bin"
) else if exist "%ROOT%\w64devkit\bin\" (
  set "PATH=%ROOT%\w64devkit\bin;%PATH%"
  echo [build.bat] Prepended toolchain: "%ROOT%\w64devkit\bin"
)

pushd "%ROOT%\build" || exit /b 1
"%ROOT%\build\premake5.exe" gmake
if errorlevel 1 (
  popd
  exit /b 1
)
popd

pushd "%ROOT%"
where mingw32-make >nul 2>nul
if not errorlevel 1 (
  mingw32-make config=release_x64 -j
  if errorlevel 1 goto fail
  goto ok
)

where make >nul 2>nul
if not errorlevel 1 (
  make config=release_x64 -j
  if errorlevel 1 goto fail
  goto ok
)

echo [build.bat] error: no mingw32-make/make on PATH
echo [build.bat] Install w64devkit to "%USERPROFILE%\Documents\CPP\w64devkit" or add mingw32-make to PATH
popd
exit /b 1

:fail
popd
exit /b 1

:ok
popd

REM Copy runtime assets beside the exe so run.bat can load resources\...
if exist "%ROOT%\resources\" (
    if exist "%ROOT%\bin\Release\zh_game.exe" (
        if not exist "%ROOT%\bin\Release\resources\" mkdir "%ROOT%\bin\Release\resources"
        xcopy /E /I /Y /Q "%ROOT%\resources" "%ROOT%\bin\Release\resources\" >nul 2>nul
    )
    if exist "%ROOT%\bin\Debug\zh_game.exe" (
        if not exist "%ROOT%\bin\Debug\resources\" mkdir "%ROOT%\bin\Debug\resources"
        xcopy /E /I /Y /Q "%ROOT%\resources" "%ROOT%\bin\Debug\resources\" >nul 2>nul
    )
)
if exist "%ROOT%\maps\" (
    if exist "%ROOT%\bin\Release\zh_game.exe" (
        if not exist "%ROOT%\bin\Release\maps\" mkdir "%ROOT%\bin\Release\maps"
        xcopy /E /I /Y /Q "%ROOT%\maps" "%ROOT%\bin\Release\maps\" >nul 2>nul
    )
    if exist "%ROOT%\bin\Debug\zh_game.exe" (
        if not exist "%ROOT%\bin\Debug\maps\" mkdir "%ROOT%\bin\Debug\maps"
        xcopy /E /I /Y /Q "%ROOT%\maps" "%ROOT%\bin\Debug\maps\" >nul 2>nul
    )
)

echo [build.bat] ok: "%ROOT%\bin\Release\zh_game.exe"
exit /b 0
