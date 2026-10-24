using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using static Fnaf4Mod.Native;

namespace Fnaf4Mod;

internal static unsafe class Mod
{
    // Window / style constants
    const uint WS_CHILD = 0x40000000;
    const uint StripStyle = 0x00C00000 | 0x00040000 | 0x00020000 | 0x00010000 | 0x00080000 | 0x01000000; // caption|thickframe|minbox|maxbox|sysmenu|maximize
    const uint StripExStyle = 0x200 | 0x100 | 0x1; // clientedge|windowedge|dlgmodalframe
    const uint SWP_NOSIZE = 1, SWP_NOMOVE = 2, SWP_NOZORDER = 4, SWP_NOACTIVATE = 0x10, SWP_FRAMECHANGED = 0x20;
    const int GWL_STYLE = -16;
    const int VK_LBUTTON = 1, VK_RBUTTON = 2, VK_SHIFT = 0x10, VK_CONTROL = 0x11, VK_LSHIFT = 0xA0, VK_LCONTROL = 0xA2;

    // Offsets inside the supported game executable (identical to the C++ version)
    const int AppPtr = 0xac9ac, RunPtr = 0xac9b4;

    static nint selfModule, gameModule, gameWindow;
    static string exePath = "", selfDir = "";
    static bool activePatch, unlockFps = true, validated, nativeHooked;
    static readonly object gate = new();

    // ---------------------------------------------------------------- startup

    [ModuleInitializer]
    internal static void Attach()
    {
        try
        {
            nint self;
            GetModuleHandleExW(0x4 | 0x2, (void*)(nint)(delegate* unmanaged[Stdcall]<int, short>)&AsyncKeyPatched, &self);
            selfModule = self;
            selfDir = Path.GetDirectoryName(ModulePath(selfModule)) ?? "";
            gameModule = GetModuleHandleW(null);
            exePath = ModulePath(gameModule);
            Log("DLL attached");
            HookModule(gameModule);
        }
        catch { }
    }

    static string ModulePath(nint module)
    {
        char* buffer = stackalloc char[1024];
        uint n = GetModuleFileNameW(module, buffer, 1024);
        return new string(buffer, 0, (int)n);
    }

    static void Log(string text)
    {
        try
        {
            using var f = new FileStream(Path.Combine(selfDir, "fnaf4-mod.log"), FileMode.Append, FileAccess.Write, FileShare.ReadWrite);
            f.Write(Encoding.UTF8.GetBytes(text + "\r\n"));
        }
        catch { }
    }

    // ------------------------------------------------------------- validation

    static void EnsureValidated()
    {
        if (validated) return;
        lock (gate)
        {
            if (validated) return;
            try
            {
                using var fs = new FileStream(exePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                long size = fs.Length;
                string hash = Convert.ToHexString(SHA256.HashData(fs));
                foreach (var p in Patches.Supported)
                    if (p.FileSize == size && string.Equals(p.Sha256, hash, StringComparison.OrdinalIgnoreCase))
                    {
                        activePatch = true;
                        break;
                    }
            }
            catch { Log("Validation open failed"); }
            Log(activePatch ? "Validated supported game" : "Game hash not recognized or hash failed");
            validated = true;
        }
    }

    static bool IsGameFile(nint file)
    {
        char* path = stackalloc char[1032];
        uint n = GetFinalPathNameByHandleW(file, path, 1032, 0 /* FILE_NAME_NORMALIZED */);
        if (n == 0 || n >= 1032) return false;
        var span = new ReadOnlySpan<char>(path, (int)n);
        if (span.StartsWith(@"\\?\")) span = span.Slice(4);
        return span.Equals(exePath, StringComparison.OrdinalIgnoreCase);
    }

    // -------------------------------------------------------- runtime patches

    static byte* App => *(byte**)((byte*)gameModule + AppPtr);
    static byte* Run => *(byte**)((byte*)gameModule + RunPtr);

    static void ApplyRuntimeSettings()
    {
        byte* app = App;
        if (app == null || *(uint*)(app + 0x10) != 112 || *(ushort*)(app + 0x14) != 0xbc82) return;
        *(ushort*)(app + 0x14) = (ushort)((*(ushort*)(app + 0x14) & ~0x800) | 0x10); // keep other flags so audio still works
        app[0x390] = 1;

        string ini = Path.Combine(selfDir, "fnaf4-mod.ini");
        fixed (char* section = "Mod", key = "UnlockFPS", file = ini)
            unlockFps = GetPrivateProfileIntW(section, key, 1, file) != 0;
        if (unlockFps) *(ushort*)(app + 0x16) &= 0xF7FF;

        InstallNativeHooks();
        Log("Applied runtime borderless settings after header checksum");
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    static void EndApplicationPatched()
    {
        try
        {
            byte* app = App, run = Run;
            if (run == null) return;
            Log($"End application action: frame={(app != null ? *(int*)(app + 0x1f0) : -1)}");
            if (app != null && *(int*)(app + 0x1f0) == 3)
            {
                *(uint*)(run + 0x38) = 4;
                *(ushort*)(run + 0x30) = 3;
                Log("Escape routed to title frame");
            }
            else *(ushort*)(run + 0x30) = 0xfffe;
        }
        catch { }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    static int TickAndRender()
    {
        byte* run = Run;
        uint before = run != null ? *(uint*)(run + 0xd0) : 0;
        int result = ((delegate* unmanaged[Cdecl]<int>)((byte*)gameModule + 0x29f40))();
        run = Run;
        if (run != null && result == 0 && *(uint*)(run + 0xd0) == before && *(uint*)(run + 0x178) == 0)
            ((delegate* unmanaged[Cdecl]<void>)((byte*)gameModule + 0x2b970))();
        return result;
    }

    static bool Unprotect(void* address, int length, out uint old)
    {
        uint o;
        int ok = VirtualProtect(address, (nuint)length, 0x40 /* PAGE_EXECUTE_READWRITE */, &o);
        old = o;
        return ok != 0;
    }

    static void Reprotect(void* address, int length, uint old)
    {
        uint ignored;
        VirtualProtect(address, (nuint)length, old, &ignored);
        FlushInstructionCache(GetCurrentProcess(), address, (nuint)length);
    }

    static void InstallNativeHooks()
    {
        if (nativeHooked) return;
        byte* entry = (byte*)gameModule + 0xbfe0;
        ReadOnlySpan<byte> signature = [0xb9, 0xfe, 0xff, 0xff, 0xff, 0x66, 0x89, 0x48, 0x30, 0xc3];
        if (entry[0] != 0xa1 || !new ReadOnlySpan<byte>(entry + 5, signature.Length).SequenceEqual(signature))
        {
            Log("Native action signature mismatch");
            return;
        }
        nativeHooked = true;

        uint old;
        if (Unprotect(entry, 5, out old))
        {
            nint target = (nint)(delegate* unmanaged[Cdecl]<void>)&EndApplicationPatched;
            entry[0] = 0xe9;
            *(int*)(entry + 1) = (int)(target - (nint)entry - 5);
            Reprotect(entry, 5, old);
            Log("Native EndApplication action hooked");
        }

        if (!unlockFps) return;

        byte* tick = (byte*)gameModule + 0x45444;
        ReadOnlySpan<byte> tickExpected = [0xe8, 0xf7, 0x4a, 0xfe, 0xff];
        if (new ReadOnlySpan<byte>(tick, 5).SequenceEqual(tickExpected) && Unprotect(tick, 5, out old))
        {
            nint target = (nint)(delegate* unmanaged[Cdecl]<int>)&TickAndRender;
            *(int*)(tick + 1) = (int)(target - (nint)tick - 5);
            Reprotect(tick, 5, old);
        }
        else
        {
            Log("FPS tick signature mismatch; FPS unlock skipped");
            return;
        }

        byte* wait = (byte*)gameModule + 0x2f21;
        ReadOnlySpan<byte> waitExpected = [0x83, 0xbe, 0xc0, 0, 0, 0, 0];
        if (new ReadOnlySpan<byte>(wait, 7).SequenceEqual(waitExpected) && Unprotect(wait, 7, out old))
        {
            // Keep the original message pump, but return before its frame-deadline wait.
            wait[0] = 0xe9;
            *(int*)(wait + 1) = 0x2f5d - 0x2f21 - 5;
            wait[5] = wait[6] = 0x90;
            Reprotect(wait, 7, old);
            Log("Rendering wait removed; simulation clock remains 60 Hz");
        }
    }

    // ------------------------------------------------------------ API hooks

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static int ReadPatched(nint file, void* buffer, uint count, uint* read, void* overlapped)
    {
        try
        {
            bool target = IsGameFile(file);
            long position = 0;
            if (target)
            {
                EnsureValidated();
                if (overlapped != null)
                {
                    var o = (OverlappedRaw*)overlapped;
                    position = (long)o->Offset | ((long)o->OffsetHigh << 32);
                }
                else
                {
                    long p;
                    if (SetFilePointerEx(file, 0, &p, 1 /* FILE_CURRENT */) != 0) position = p;
                    else target = false;
                }
            }
            if (target && activePatch && position == 1554554) ApplyRuntimeSettings();
        }
        catch { }
        return ReadFile(file, buffer, count, read, overlapped);
    }

    static void Bounds(nint window, out int x, out int y, out int w, out int h)
    {
        MonitorInfo m = default;
        m.Size = (uint)sizeof(MonitorInfo);
        GetMonitorInfoW(MonitorFromWindow(window, 2 /* MONITOR_DEFAULTTONEAREST */), &m);
        int mw = m.Monitor.Right - m.Monitor.Left, mh = m.Monitor.Bottom - m.Monitor.Top;
        w = mw; h = mw * 9 / 16;
        if (h > mh) { h = mh; w = mh * 16 / 9; }
        x = m.Monitor.Left + (mw - w) / 2;
        y = m.Monitor.Top + (mh - h) / 2;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static int PositionPatched(nint window, nint after, int x, int y, int w, int h, uint flags)
    {
        try
        {
            if (window == gameWindow && activePatch && IsIconic(window) == 0)
            {
                Bounds(window, out x, out y, out w, out h);
                flags &= ~(SWP_NOSIZE | SWP_NOMOVE);
            }
        }
        catch { }
        return SetWindowPos(window, after, x, y, w, h, flags);
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static nint CreatePatched(uint ex, char* cls, char* title, uint style, int x, int y, int w, int h,
        nint parent, nint menu, nint instance, void* param)
    {
        bool main = false;
        try
        {
            Log("Create window reached");
            main = activePatch && parent == 0 && (style & WS_CHILD) == 0 && w >= 500 && h >= 300;
            if (main) { style &= ~StripStyle; ex &= ~StripExStyle; }
        }
        catch { }
        nint window = CreateWindowExW(ex, cls, title, style, x, y, w, h, parent, menu, instance, param);
        if (main) { gameWindow = window; Log("Game window found"); }
        return window;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static int ShowPatched(nint window, int mode)
    {
        int result = ShowWindow(window, mode);
        try
        {
            // SW_HIDE = 0, SW_SHOWMINIMIZED = 2, SW_MINIMIZE = 6
            if (window == gameWindow && activePatch && mode != 0 && mode != 6 && mode != 2)
            {
                Bounds(window, out int x, out int y, out int w, out int h);
                SetWindowLongW(window, GWL_STYLE, GetWindowLongW(window, GWL_STYLE) & ~(int)StripStyle);
                SetWindowPos(window, 0, x, y, w, h, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
            }
        }
        catch { }
        return result;
    }

    // ----------------------------------------------------- mouse -> Ctrl/Shift

    static bool InNight()
    {
        if (!activePatch) return false;
        byte* app = App;
        return app != null && *(int*)(app + 0x1f0) == 3;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static short AsyncKeyPatched(int key)
    {
        short state = GetAsyncKeyState(key);
        if (InNight())
        {
            if (key == VK_CONTROL || key == VK_LCONTROL) state = unchecked((short)(state | (GetAsyncKeyState(VK_LBUTTON) & 0x8000)));
            if (key == VK_SHIFT || key == VK_LSHIFT) state = unchecked((short)(state | (GetAsyncKeyState(VK_RBUTTON) & 0x8000)));
        }
        return state;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static short KeyPatched(int key)
    {
        short state = GetKeyState(key);
        if (InNight())
        {
            if (key == VK_CONTROL || key == VK_LCONTROL) state = unchecked((short)(state | (GetKeyState(VK_LBUTTON) & 0x8000)));
            if (key == VK_SHIFT || key == VK_LSHIFT) state = unchecked((short)(state | (GetKeyState(VK_RBUTTON) & 0x8000)));
        }
        return state;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static int KeyboardPatched(byte* keys)
    {
        int ok = GetKeyboardState(keys);
        if (ok != 0 && InNight())
        {
            keys[VK_CONTROL] |= (byte)(keys[VK_LBUTTON] & 0x80);
            keys[VK_LCONTROL] |= (byte)(keys[VK_LBUTTON] & 0x80);
            keys[VK_SHIFT] |= (byte)(keys[VK_RBUTTON] & 0x80);
            keys[VK_LSHIFT] |= (byte)(keys[VK_RBUTTON] & 0x80);
        }
        return ok;
    }

    // --------------------------------------------- follow modules the game loads

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static nint LoadPatchedW(char* path)
    {
        nint m = LoadLibraryW(path);
        if (m != 0) HookModule(m);
        return m;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static nint LoadPatchedA(byte* path)
    {
        nint m = LoadLibraryA(path);
        if (m != 0) HookModule(m);
        return m;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static nint LoadExPatchedW(char* path, nint file, uint flags)
    {
        nint m = LoadLibraryExW(path, file, flags);
        if (m != 0 && (flags & (0x2 | 0x20)) == 0) HookModule(m); // not DATAFILE / IMAGE_RESOURCE
        return m;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    static nint LoadExPatchedA(byte* path, nint file, uint flags)
    {
        nint m = LoadLibraryExA(path, file, flags);
        if (m != 0 && (flags & (0x2 | 0x20)) == 0) HookModule(m);
        return m;
    }

    static void HookModule(nint module)
    {
        try
        {
            if (module == selfModule) return;
            if (module != gameModule && !ModulePath(module).Contains("mrt", StringComparison.Ordinal)) return;
            Log("Hooking game/runtime module");

            Hook(module, "ReadFile"u8, (nint)(delegate* unmanaged[Stdcall]<nint, void*, uint, uint*, void*, int>)&ReadPatched);
            Hook(module, "GetAsyncKeyState"u8, (nint)(delegate* unmanaged[Stdcall]<int, short>)&AsyncKeyPatched);
            Hook(module, "GetKeyState"u8, (nint)(delegate* unmanaged[Stdcall]<int, short>)&KeyPatched);
            Hook(module, "GetKeyboardState"u8, (nint)(delegate* unmanaged[Stdcall]<byte*, int>)&KeyboardPatched);
            Hook(module, "CreateWindowExW"u8, (nint)(delegate* unmanaged[Stdcall]<uint, char*, char*, uint, int, int, int, int, nint, nint, nint, void*, nint>)&CreatePatched);
            Hook(module, "ShowWindow"u8, (nint)(delegate* unmanaged[Stdcall]<nint, int, int>)&ShowPatched);
            Hook(module, "SetWindowPos"u8, (nint)(delegate* unmanaged[Stdcall]<nint, nint, int, int, int, int, uint, int>)&PositionPatched);
            Hook(module, "LoadLibraryW"u8, (nint)(delegate* unmanaged[Stdcall]<char*, nint>)&LoadPatchedW);
            Hook(module, "LoadLibraryA"u8, (nint)(delegate* unmanaged[Stdcall]<byte*, nint>)&LoadPatchedA);
            Hook(module, "LoadLibraryExW"u8, (nint)(delegate* unmanaged[Stdcall]<char*, nint, uint, nint>)&LoadExPatchedW);
            Hook(module, "LoadLibraryExA"u8, (nint)(delegate* unmanaged[Stdcall]<byte*, nint, uint, nint>)&LoadExPatchedA);
        }
        catch { }
    }

    /// <summary>Overwrites the first import-table entry named <paramref name="name"/> (PE32 / x86).</summary>
    static void Hook(nint module, ReadOnlySpan<byte> name, nint replacement)
    {
        byte* b = (byte*)module;
        byte* nt = b + *(int*)(b + 0x3c);
        uint rva = *(uint*)(nt + 128); // DataDirectory[IMPORT].VirtualAddress
        if (rva == 0) return;

        // IMAGE_IMPORT_DESCRIPTOR = 5 x uint: OriginalFirstThunk, TimeDateStamp, ForwarderChain, Name, FirstThunk
        for (uint* d = (uint*)(b + rva); d[3] != 0; d += 5)
        {
            if (d[0] == 0) continue;
            uint* names = (uint*)(b + d[0]);
            uint* table = (uint*)(b + d[4]);
            for (; *names != 0; names++, table++)
            {
                if ((*names & 0x80000000) != 0) continue; // imported by ordinal
                byte* entry = b + *names + 2;              // skip IMAGE_IMPORT_BY_NAME.Hint
                if (!MemoryMarshal.CreateReadOnlySpanFromNullTerminated(entry).SequenceEqual(name)) continue;

                uint old;
                if (VirtualProtect(table, 4, 0x04 /* PAGE_READWRITE */, &old) != 0)
                {
                    *table = (uint)replacement;
                    uint ignored;
                    VirtualProtect(table, 4, old, &ignored);
                }
                return;
            }
        }
    }
}
