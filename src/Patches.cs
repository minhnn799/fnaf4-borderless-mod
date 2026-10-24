namespace Fnaf4Mod;

internal readonly record struct PatchSet(long FileSize, string Sha256);

internal static class Patches
{
    // Supported FNAF 4 executables (v1.1 and Halloween Edition). Anything else is left unpatched.
    public static readonly PatchSet[] Supported =
    {
        new(518464345L, "c3548a612870c9a4508ba8420dc2108f46b896c747866c49d7e8df3a0499acc1"),
        new(522913104L, "befc408f7e15576aaa3aa66ba65e8d4daf3c2f373477415595e8c0a84cc51aef"),
    };
}
