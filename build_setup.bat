@echo off
rem ===== dpeaklim installer build (embeds bin\dpeaklim_gui.exe + inpoutx64 driver) =====
setlocal
set "HERE=%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [ERROR] vcvars64.bat not found. & exit /b 1 )
if not exist "%HERE%obj" mkdir "%HERE%obj"

pushd "%HERE%setup"
rc /nologo /fo payload.res payload.rc
if errorlevel 1 ( echo [RC FAILED] & popd & exit /b 1 )

cl /nologo /EHsc /O2 /MT /W3 /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /Fe:"%HERE%bin\dpeaklim_setup.exe" /Fo:"%HERE%obj\\" ^
  setup.cpp payload.res ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup ^
  user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib ole32.lib ^
  /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'" /MANIFESTINPUT:setup.manifest
set ERR=%errorlevel%
popd
if not "%ERR%"=="0" ( echo [BUILD FAILED] & exit /b 1 )
echo [SETUP BUILD OK] -^> %HERE%bin\dpeaklim_setup.exe
