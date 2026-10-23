@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 (
  echo Run this from an x86 Native Tools Command Prompt for Visual Studio.
  exit /b 1
)
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /LD /MT /O2 /EHsc /W3 /Fo:build\mod.obj mod.cpp /link /MACHINE:X86 /DEF:winmm.def /OUT:build\winmm.dll /IMPLIB:build\winmm.lib
exit /b %errorlevel%

