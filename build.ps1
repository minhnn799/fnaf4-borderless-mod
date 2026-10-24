$ErrorActionPreference='Stop'
Push-Location $PSScriptRoot
try {
    $vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(!$vs){throw 'Install Visual Studio C++ Build Tools and Windows SDK.'}
    $vc=(Get-ChildItem "$vs/VC/Tools/MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
    $kits="${env:ProgramFiles(x86)}/Windows Kits/10"
    $sdk=(Get-ChildItem "$kits/Include" -Directory | Where-Object { (Test-Path "$($_.FullName)/um/Windows.h") -and (Test-Path "$kits/Lib/$($_.Name)/um/x86/kernel32.lib") } | Sort-Object Name -Descending | Select-Object -First 1).Name
    if(!$sdk){throw 'A complete Windows SDK is required.'}
    $env:INCLUDE="$vc/include;$kits/Include/$sdk/ucrt;$kits/Include/$sdk/shared;$kits/Include/$sdk/um"
    $env:LIB="$vc/lib/x86;$kits/Lib/$sdk/ucrt/x86;$kits/Lib/$sdk/um/x86"
    $env:PATH="$vc/bin/Hostx64/x86;$env:PATH"
    & "$vc/bin/Hostx64/x86/cl.exe" /nologo /c /MT /O2 /W4 /Fo:native/bootstrap.obj native/bootstrap.cpp
    if($LASTEXITCODE){throw 'Native bootstrap compilation failed.'}
    $dotnet='dotnet'
    if($env:DOTNET_EXE){$dotnet=$env:DOTNET_EXE}
    elseif(Test-Path '../../work/dotnet10-sdk/dotnet.exe'){$dotnet=(Resolve-Path '../../work/dotnet10-sdk/dotnet.exe').Path}
    & $dotnet publish Fnaf4Mod.csproj -c Release -r win-x86 -o build -p:IlcUseEnvironmentalTools=true
    if($LASTEXITCODE){throw 'NativeAOT publish failed.'}
} finally { Pop-Location }
