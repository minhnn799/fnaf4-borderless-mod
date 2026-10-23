# FNAF 4 Borderless Mod

A Windows mod for Five Nights at Freddy's 4.

- Borderless, non-exclusive fullscreen in 16:9 (stretches the original view).
- Escape returns to the menu during a night.
- Hold left mouse for the flashlight and right mouse to close doors. Ctrl and Shift still work.
- Optional FPS unlocking.

**Experimental alpha:** FPS unlocking, mouse controls, and Escape-to-menu still need gameplay testing.

## Installation

1. Download the ZIP from [Releases](https://github.com/minhnn799/fnaf4-borderless-mod/releases).
2. Close the game.
3. Extract `winmm.dll` and `fnaf4-mod.ini` into the same folder as the game EXE.
4. Start the game normally.

If another mod already uses `winmm.dll`, do not overwrite it. The mods cannot be combined automatically.

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

## Build from source

With Visual Studio C++ build tools and a Windows SDK installed, run `build.cmd` from an **x86 Native Tools Command Prompt**. Output: `build/winmm.dll`.
