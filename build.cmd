@echo off
setlocal
where dotnet >nul 2>nul
if errorlevel 1 (
  echo .NET 8 SDK or newer is required.
  exit /b 1
)
cd /d "%~dp0"
rem NativeAOT also needs "Desktop development with C++" (MSVC + Windows SDK) installed.
dotnet publish Fnaf4Mod.csproj -c Release -r win-x86 -o build
exit /b %errorlevel%
