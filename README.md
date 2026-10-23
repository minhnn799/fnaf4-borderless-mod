# FNAF 4 Borderless Mod

An experimental, drop-in Windows mod for the original Five Nights at Freddy's 4. No launcher or Unity remake is required.

## Status

**v0.1.0-alpha.1 — experimental prerelease.** The DLL builds successfully. Borderless 1920×1080 was observed in an earlier build. The latest FPS, mouse controls, and Escape changes have not been play-tested. FPS and timing measurements are still needed; this is not a verified stable release.

## Features

- Non-exclusive borderless fullscreen with a centered 16:9 window. The original 4:3 view is stretched, not expanded or cropped.
- Escape during a night requests the original title menu.
- Hold left mouse for the flashlight and right mouse to close doors. Ctrl and Shift remain available.
- Experimental uncapped drawing, enabled by default. The patch removes the rendering wait and game VSync and draws between updates, retaining the original 60 Hz simulation clock. Actual FPS depends on the renderer, compositor, hardware, and driver settings. Original animation assets still have their original frame counts.

## Install

1. Close the game.
2. Extract `winmm.dll` and `fnaf4-mod.ini` from the release ZIP beside your original game EXE.
3. Launch the original EXE normally. Windows loads the proxy through the game's existing WinMM import.

Do not overwrite an existing `winmm.dll` from another mod. Proxy chaining is not supported. The original game executable is not modified on disk.

To disable FPS unlocking, close the game and set `UnlockFPS=0` in `fnaf4-mod.ini`. To uninstall, close the game and remove the two installed files. Diagnostic messages are written to `fnaf4-mod.log` beside the DLL.

## Compatibility

Windows, x86 game executable. Only the following exact supplied builds are recognized by SHA-256; other versions are left unpatched:

| Edition | SHA-256 |
| --- | --- |
| Halloween Edition | `c3548a612870c9a4508ba8420dc2108f46b896c747866c49d7e8df3a0499acc1` |
| Standard v1.1 | `befc408f7e15576aaa3aa66ba65e8d4daf3c2f373477415595e8c0a84cc51aef` |

The standard edition has not been runtime-tested. No game executables, assets, saves, or game data are included. You need your own copy of FNAF 4. This is an unofficial fan mod and is not affiliated with the game's creators.

## Build

Install Visual Studio C++ build tools and a Windows SDK. Open an **x86 Native Tools Command Prompt**, then run `build.cmd`. The output is `build/winmm.dll`. The project uses MSVC x86 inline assembly and statically links the C++ runtime.

## Reporting problems

Include the game edition, Windows version, whether `UnlockFPS` was enabled, and steps to reproduce. Check logs for personal paths before sharing them. Please report measured FPS and any changes to night duration, movement, audio, or animations.
