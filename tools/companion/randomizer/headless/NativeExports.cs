// C ABI for embedding the generator in the Android/3DS-side tooling (NativeAOT shared library).
// Build with -p:SomRandoNativeLib=1 -p:PublishAot=true -p:NativeLib=Shared. Original glue code of this repository.
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;

public static unsafe class NativeExports
{
    static string S(byte* p) => p == null ? null : Marshal.PtrToStringUTF8((IntPtr)p);

    // options: newline separated key=value lines (may be null). Returns 0 on success; otherwise err (UTF-8, NUL terminated) holds the message.
    [UnmanagedCallersOnly(EntryPoint = "somrando_generate")]
    public static int Generate(byte* mode, byte* seed, byte* src, byte* dst, byte* difficulty, byte* options, byte* workdir, byte* err, int errLen)
    {
        var sets = new Dictionary<string, string>();
        string o = S(options);
        if (o != null)
            foreach (var line in o.Split('\n'))
            {
                int eq = line.IndexOf('=');
                if (eq > 0) sets[line.Substring(0, eq).Trim()] = line.Substring(eq + 1).Trim();
            }
        string wd = S(workdir);
        if (wd != null) { Directory.CreateDirectory(wd); Directory.SetCurrentDirectory(wd); }
        string e = SomRandoApi.Generate(S(mode), S(seed), S(src), S(dst), S(difficulty) ?? "casual", sets, false);
        if (e == null) return 0;
        if (err != null && errLen > 0)
        {
            var b = System.Text.Encoding.UTF8.GetBytes(e);
            int n = Math.Min(b.Length, errLen - 1);
            for (int i = 0; i < n; i++) err[i] = b[i];
            err[n] = 0;
        }
        return 1;
    }

    [UnmanagedCallersOnly(EntryPoint = "somrando_version")]
    public static byte* Version() => (byte*)Marshal.StringToCoTaskMemUTF8(SomRandoApi.Version);
}
