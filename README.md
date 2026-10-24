# FNAF 4 Borderless Mod

A Windows mod for Five Nights at Freddy's 4.

- Borderless, non-exclusive fullscreen in 16:9 (stretches the original view).
- Escape returns to the menu during a night.
- Hold left mouse for the flashlight and right mouse to close doors. Ctrl and Shift still work.
- Optional FPS unlocking.

## Installation

1. Download the ZIP from [Releases](https://github.com/minhnn799/fnaf4-borderless-mod/releases).
2. Close the game.
3. Extract `winmm.dll` and `fnaf4-mod.ini` into the same folder as the game EXE.
4. Start the game normally.

## Settings

In `fnaf4-mod.ini`:

```ini
[Mod]
UnlockFPS=1
```

Set `UnlockFPS=0` to disable FPS unlocking. Restart the game after changing settings.

## Uninstall

Close the game and remove `winmm.dll` and `fnaf4-mod.ini`.

## Compatibility

Supports specific Windows builds of FNAF 4 v1.1 and Halloween Edition. Other executable versions are left unpatched. The standard edition has not been play-tested.

A copy of the original game is required. No game files are included.

## Build from source (C#)

This uses C# (.NET 10 NativeAOT) for the mod logic and a small C++ bootstrap for early initialization and WinMM forwarding. It produces one native 32-bit `winmm.dll`; players do not need the .NET SDK or runtime installed.

Requirements: .NET 10 SDK, plus Visual Studio "Desktop development with C++" (MSVC + Windows SDK), which NativeAOT needs for linking.

```
build.cmd
```

Output: `build/winmm.dll`. Copy it and `fnaf4-mod.ini` next to the game EXE.

Layout: `src/Mod.cs` (hooks and patches), `src/Native.cs` (Win32 imports), `src/Patches.cs` (supported EXE hashes), `native/bootstrap.cpp` and `native/proxy-exports.h` (native startup bridge and 192 WinMM forwarding stubs), and `winmm.def` (export names and ordinals).
