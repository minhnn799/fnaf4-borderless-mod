using System.Runtime.InteropServices;

namespace Fnaf4Mod;

[StructLayout(LayoutKind.Sequential)]
internal struct Rect { public int Left, Top, Right, Bottom; }

[StructLayout(LayoutKind.Sequential)]
internal struct MonitorInfo { public uint Size; public Rect Monitor; public Rect Work; public uint Flags; }

[StructLayout(LayoutKind.Sequential)]
internal struct OverlappedRaw { public nuint Internal, InternalHigh; public uint Offset, OffsetHigh; public nint Event; }

internal static unsafe class Native
{
    const string K = "kernel32.dll";
    const string U = "user32.dll";

    [DllImport(K, ExactSpelling = true)] public static extern nint GetModuleHandleW(char* name);
    [DllImport(K, ExactSpelling = true)] public static extern int GetModuleHandleExW(uint flags, void* address, nint* module);
    [DllImport(K, ExactSpelling = true)] public static extern uint GetModuleFileNameW(nint module, char* buffer, uint size);
    [DllImport(K, ExactSpelling = true)] public static extern int ReadFile(nint file, void* buffer, uint count, uint* read, void* overlapped);
    [DllImport(K, ExactSpelling = true)] public static extern int SetFilePointerEx(nint file, long distance, long* newPosition, uint method);
    [DllImport(K, ExactSpelling = true)] public static extern uint GetFinalPathNameByHandleW(nint file, char* path, uint size, uint flags);
    [DllImport(K, ExactSpelling = true)] public static extern int VirtualProtect(void* address, nuint size, uint protect, uint* old);
    [DllImport(K, ExactSpelling = true)] public static extern int FlushInstructionCache(nint process, void* address, nuint size);
    [DllImport(K, ExactSpelling = true)] public static extern nint GetCurrentProcess();
    [DllImport(K, ExactSpelling = true)] public static extern uint GetPrivateProfileIntW(char* section, char* key, int fallback, char* file);
    [DllImport(K, ExactSpelling = true)] public static extern nint LoadLibraryW(char* path);
    [DllImport(K, ExactSpelling = true)] public static extern nint LoadLibraryA(byte* path);
    [DllImport(K, ExactSpelling = true)] public static extern nint LoadLibraryExW(char* path, nint file, uint flags);
    [DllImport(K, ExactSpelling = true)] public static extern nint LoadLibraryExA(byte* path, nint file, uint flags);

    [DllImport(U, ExactSpelling = true)] public static extern short GetAsyncKeyState(int key);
    [DllImport(U, ExactSpelling = true)] public static extern short GetKeyState(int key);
    [DllImport(U, ExactSpelling = true)] public static extern int GetKeyboardState(byte* keys);
    [DllImport(U, ExactSpelling = true)] public static extern nint MonitorFromWindow(nint window, uint flags);
    [DllImport(U, ExactSpelling = true)] public static extern int GetMonitorInfoW(nint monitor, MonitorInfo* info);
    [DllImport(U, ExactSpelling = true)] public static extern int IsIconic(nint window);
    [DllImport(U, ExactSpelling = true)] public static extern int GetWindowLongW(nint window, int index);
    [DllImport(U, ExactSpelling = true)] public static extern int SetWindowLongW(nint window, int index, int value);
    [DllImport(U, ExactSpelling = true)] public static extern int SetWindowPos(nint window, nint after, int x, int y, int w, int h, uint flags);
    [DllImport(U, ExactSpelling = true)] public static extern int ShowWindow(nint window, int mode);
    [DllImport(U, ExactSpelling = true)]
    public static extern nint CreateWindowExW(uint ex, char* cls, char* title, uint style, int x, int y, int w, int h,
        nint parent, nint menu, nint instance, void* param);
}
