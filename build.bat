@echo off
rem ===== dpeaklim GUI command-line build (MSVC) =====
setlocal
set "HERE=%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
  echo [ERROR] vcvars64.bat not found. Adjust the path in build.bat.
  exit /b 1
)

if not exist "%HERE%obj" mkdir "%HERE%obj"
if not exist "%HERE%bin" mkdir "%HERE%bin"

cl /nologo /EHsc /O2 /MT /W3 /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /Fe:"%HERE%bin\dpeaklim_gui.exe" /Fo:"%HERE%obj\\" ^
  "%HERE%src\gui.cpp" "%HERE%src\dpeaklim.cpp" "%HERE%src\InpOut32.cpp" "%HERE%src\stdafx.cpp" ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup ^
  user32.lib gdi32.lib comctl32.lib advapi32.lib winmm.lib shell32.lib ^
  /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'" /MANIFESTINPUT:"%HERE%src\app.manifest"

if errorlevel 1 (
  echo.
  echo [BUILD FAILED]
  exit /b 1
)
echo.
echo [BUILD OK] -^> %HERE%bin\dpeaklim_gui.exe
