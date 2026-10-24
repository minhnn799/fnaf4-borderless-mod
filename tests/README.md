# Native x86 smoke test

Build the mod with build.cmd. In an x86 Visual Studio developer prompt:

```
cl /nologo /MT /O2 /Fe:build\smoke.exe /Fo:build\smoke.obj tests\smoke.cpp
build\smoke.exe "%CD%\build\winmm.dll"
```

This loads the native DLL into a disposable x86 test process, triggers the early ReadFile bootstrap, and checks timeGetTime and waveOutGetNumDevs forwarding. The log must show that the test executable is not a supported game. It does not launch FNAF or validate gameplay behavior.

Verified locally: PE x86 architecture, all 192 original export names and ordinals, successful bootstrap and smoke test. No .NET runtime DLL is required by the output. The C# build still needs manual gameplay testing before replacing the previously tested C++ release.
